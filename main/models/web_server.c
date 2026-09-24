/* web_server.c -- the web-server execution model: an esp_http_server in front of PHP, run fresh per
 * request (shared-nothing). Moved out of main.c; the former ws_* names are web_* now,
 * so ws_* is free for the real WebSocket support coming later. Empty TU unless PHP_PROJECT_WEB_SERVER. */
#ifdef PHP_PROJECT_WEB_SERVER

#include <stdlib.h>       /* realloc (the per-request output buffer) */
#include <unistd.h>       /* access / R_OK (resolving the init script) */

#include "esp_log.h"      /* ESP_LOGI / ESP_LOGE */

#include "boot.h"         /* PHP_TASK_CORE / HTTPD_TASK_CORE (httpd task pinning) */
#include "app.h"          /* s_board_ip, register_esp32_server_vars */
#include "php_task.h"     /* run_php_file */
#include "web_server.h"

static const char *TAG = "php-esp32";

#include <strings.h>          /* strncasecmp */
#include <stdio.h>            /* fopen/fread (static files) */
#include <sys/stat.h>         /* stat / S_ISREG (static files) */
#include "freertos/FreeRTOS.h"   /* must precede semphr.h (SemaphoreHandle_t + the guard) */
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"     /* getpeername (REMOTE_ADDR) */
#include "lwip/inet.h"        /* inet_ntop */
#include "php_main.h"         /* php_request_startup / php_request_shutdown / sapi_send_headers */
#include "php_variables.h"    /* php_register_variable */
#include "SAPI.h"             /* sapi_module, sapi_header_struct, SG() */

/*
 * The web-server execution model. A C HTTP server (esp_http_server) sits in front and PHP is run
 * fresh for each request -- shared-nothing, the way a script runs behind Apache/nginx + PHP-FPM.
 * This is a small SAPI: the incoming HTTP request (method, URI, query, headers, cookies, POST body)
 * is turned into a full CGI-style $_SERVER / $_GET / $_POST / $_COOKIE, the front controller runs,
 * and the script's output plus the headers/status/cookies it set become the HTTP response. That is
 * enough to drive a real framework (Laravel, ...) as a browsable app -- routing, sessions, forms.
 * Selected at build time with -DPHP_PROJECT_WEB_SERVER=ON (the `web-server` project type); the
 * default build uses the run-script + setup()/loop() model.
 *
 * PHP runs in php_task (which already has the big 64 KB stack the compiler needs), NOT in the
 * httpd task: the httpd handler parses the request off the socket, parks it, wakes php_task, and
 * waits. So the httpd task keeps a small stack, PHP always runs on the stack it was set up with,
 * and neither task touches the socket while the other is using it. The httpd server handles one
 * request at a time, so the single shared slot below is safe.
 */
static const char *s_web_script;
static char  *s_web_out;              /* per-request output buffer (grows as needed) */
static size_t s_web_len, s_web_cap;
static httpd_req_t *s_web_req;         /* the request php_task should serve */
static bool   s_web_headers_done;      /* did web_send_headers() run for this request? */
static SemaphoreHandle_t s_web_req_ready;   /* httpd -> php_task: a request is waiting */
static SemaphoreHandle_t s_web_resp_ready;  /* php_task -> httpd: the response is ready */

/* One parsed request. All strings are static and live for the whole request cycle, so they can be
 * handed to SG(request_info) (which core does not free) and to httpd_resp_set_hdr (which stores
 * pointers, not copies). Only one request is in flight at a time, so a single instance is fine. */
typedef struct {
    int         method;               /* HTTP_GET / HTTP_POST / ... */
    const char *method_str;           /* "GET" / "POST" / ... */
    char        uri[1024];            /* full request-target, e.g. "/foo?bar=1" (REQUEST_URI) */
    char        path[1024];           /* just the path, no query (request_uri / PHP_SELF base) */
    char       *query;                /* into uri after '?', or "" */
    char        host[192];            /* Host header */
    char        server_name[192];     /* Host without :port */
    char        cookie[1024];         /* Cookie header (-> $_COOKIE) */
    char        ctype[192];           /* Content-Type (-> POST parsing) */
    char        useragent[256];
    char        accept[256];
    char        accept_lang[128];
    char        referer[256];
    char        xrw[64];              /* X-Requested-With (Laravel ajax detection) */
    char        authorization[512];
    char        remote_addr[48];
    char        remote_port[8];
    char        server_addr[48];      /* the board's IP */
    char       *body;                 /* POST body (malloc'd, freed after send), or NULL */
    size_t      body_len;
    size_t      body_pos;             /* consumed by web_read_post */
} web_request_t;
static web_request_t s_req;

/* The document root (dirname of the entry script, e.g. /sdcard/public) -- constant for the run.
 * Computed once at startup; used for static-file serving and $_SERVER['DOCUMENT_ROOT']. */
static char s_docroot[256];

/* Captured response headers. httpd_resp_set_hdr stores the pointers we pass, and PHP frees its own
 * header strings at request shutdown, so we copy each "Field\0value" into buffers that outlive the
 * send (which happens back in the httpd task, after php_request_shutdown). */
#define WEB_MAX_HDR 24
static char s_status_line[48];
static char s_ctype_hdr[192];
static char s_hdr_store[WEB_MAX_HDR][320];

static const char *web_method_str(int m)
{
    switch (m) {
        case HTTP_GET:     return "GET";
        case HTTP_POST:    return "POST";
        case HTTP_PUT:     return "PUT";
        case HTTP_PATCH:   return "PATCH";
        case HTTP_DELETE:  return "DELETE";
        case HTTP_HEAD:    return "HEAD";
        case HTTP_OPTIONS: return "OPTIONS";
        default:           return "GET";
    }
}

static const char *web_reason(int code)
{
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 419: return "Page Expired";
        case 422: return "Unprocessable Content";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default:  return "Status";
    }
}

/* ub_write sink (runs in php_task): append script output to the response buffer. */
static size_t web_ub_write(const char *str, size_t len)
{
    if (s_web_len + len > s_web_cap) {
        size_t ncap = (s_web_len + len) * 2 + 1024;
        char *n = realloc(s_web_out, ncap);
        if (!n) {
            return len;   /* drop output under OOM rather than fail the write */
        }
        s_web_out = n;
        s_web_cap = ncap;
    }
    memcpy(s_web_out + s_web_len, str, len);
    s_web_len += len;
    return len;
}

/* read_cookies hook: hand PHP the raw Cookie header so it can build $_COOKIE (sessions). */
static char *web_read_cookies(void)
{
    return s_req.cookie[0] ? s_req.cookie : NULL;
}

/* read_post hook: feed PHP the POST body it needs for $_POST / php://input. */
static size_t web_read_post(char *buffer, size_t count)
{
    size_t avail = s_req.body_len - s_req.body_pos;
    size_t n = count < avail ? count : avail;
    if (n) {
        memcpy(buffer, s_req.body + s_req.body_pos, n);
        s_req.body_pos += n;
    }
    return n;
}

/* register_server_variables hook: build a full CGI-style $_SERVER for the front controller. */
static void web_register_server_vars(zval *arr)
{
    php_register_variable("REQUEST_METHOD", s_req.method_str, arr);
    php_register_variable("REQUEST_URI", s_req.uri, arr);
    php_register_variable("QUERY_STRING", s_req.query, arr);
    php_register_variable("SCRIPT_NAME", "/index.php", arr);
    php_register_variable("PHP_SELF", "/index.php", arr);
    php_register_variable("SCRIPT_FILENAME", (char *) s_web_script, arr);
    php_register_variable("DOCUMENT_ROOT", s_docroot, arr);
    php_register_variable("SERVER_PROTOCOL", "HTTP/1.1", arr);
    php_register_variable("GATEWAY_INTERFACE", "CGI/1.1", arr);
    php_register_variable("SERVER_SOFTWARE", "php-esp32", arr);
    php_register_variable("SERVER_NAME", s_req.server_name, arr);
    php_register_variable("SERVER_PORT", "80", arr);
    php_register_variable("SERVER_ADDR", s_req.server_addr, arr);
    php_register_variable("REMOTE_ADDR", s_req.remote_addr, arr);
    php_register_variable("REMOTE_PORT", s_req.remote_port, arr);
    if (s_req.host[0])          php_register_variable("HTTP_HOST", s_req.host, arr);
    if (s_req.useragent[0])     php_register_variable("HTTP_USER_AGENT", s_req.useragent, arr);
    if (s_req.accept[0])        php_register_variable("HTTP_ACCEPT", s_req.accept, arr);
    if (s_req.accept_lang[0])   php_register_variable("HTTP_ACCEPT_LANGUAGE", s_req.accept_lang, arr);
    if (s_req.cookie[0])        php_register_variable("HTTP_COOKIE", s_req.cookie, arr);
    if (s_req.referer[0])       php_register_variable("HTTP_REFERER", s_req.referer, arr);
    if (s_req.xrw[0])           php_register_variable("HTTP_X_REQUESTED_WITH", s_req.xrw, arr);
    if (s_req.authorization[0]) php_register_variable("HTTP_AUTHORIZATION", s_req.authorization, arr);
    if (s_req.ctype[0])         php_register_variable("CONTENT_TYPE", s_req.ctype, arr);
    if (s_req.body_len) {
        char cl[16];
        snprintf(cl, sizeof cl, "%zu", s_req.body_len);
        php_register_variable("CONTENT_LENGTH", cl, arr);
    }
    register_esp32_server_vars(arr);
}

/* send_headers hook (runs in php_task): translate the headers/status the script set into the httpd
 * response. Copies each header so it survives request shutdown before the httpd task sends. */
static int web_send_headers(sapi_headers_struct *h)
{
    int code = h->http_response_code ? h->http_response_code : 200;
    snprintf(s_status_line, sizeof s_status_line, "%d %s", code, web_reason(code));
    httpd_resp_set_status(s_web_req, s_status_line);

    bool have_ctype = false;
    int slot = 0;
    zend_llist_position pos;
    sapi_header_struct *hh = zend_llist_get_first_ex(&h->headers, &pos);
    while (hh) {
        const char *line = hh->header;
        const char *colon = line ? strchr(line, ':') : NULL;
        if (colon) {
            size_t flen = (size_t) (colon - line);
            const char *val = colon + 1;
            while (*val == ' ') {
                val++;
            }
            if (flen == 12 && strncasecmp(line, "Content-type", 12) == 0) {
                snprintf(s_ctype_hdr, sizeof s_ctype_hdr, "%s", val);
                httpd_resp_set_type(s_web_req, s_ctype_hdr);
                have_ctype = true;
            } else if (slot < WEB_MAX_HDR) {
                char *buf = s_hdr_store[slot];
                if (flen > 200) {
                    flen = 200;
                }
                memcpy(buf, line, flen);
                buf[flen] = '\0';
                char *vbuf = buf + flen + 1;
                snprintf(vbuf, sizeof s_hdr_store[slot] - flen - 1, "%s", val);
                httpd_resp_set_hdr(s_web_req, buf, vbuf);   /* field/value both persist in buf */
                slot++;
            }
        }
        hh = zend_llist_get_next_ex(&h->headers, &pos);
    }
    if (!have_ctype) {
        httpd_resp_set_type(s_web_req, "text/html; charset=UTF-8");
    }
    s_web_headers_done = true;
    return SAPI_HEADER_SENT_SUCCESSFULLY;
}

/* Read one request header into a fixed buffer (empty string if absent). */
static void web_get_hdr(httpd_req_t *req, const char *name, char *buf, size_t sz)
{
    buf[0] = '\0';
    httpd_req_get_hdr_value_str(req, name, buf, sz);   /* leaves buf "" if not found */
}

/* Fill REMOTE_ADDR / REMOTE_PORT from the peer socket. */
static void web_peer(httpd_req_t *req)
{
    snprintf(s_req.remote_addr, sizeof s_req.remote_addr, "0.0.0.0");
    snprintf(s_req.remote_port, sizeof s_req.remote_port, "0");
    int fd = httpd_req_to_sockfd(req);
    if (fd < 0) {
        return;
    }
    struct sockaddr_in6 sa;
    socklen_t sl = sizeof sa;
    if (getpeername(fd, (struct sockaddr *) &sa, &sl) != 0) {
        return;
    }
    if (sa.sin6_family == AF_INET6) {
        inet_ntop(AF_INET6, &sa.sin6_addr, s_req.remote_addr, sizeof s_req.remote_addr);
        snprintf(s_req.remote_port, sizeof s_req.remote_port, "%u", ntohs(sa.sin6_port));
        if (strncmp(s_req.remote_addr, "::ffff:", 7) == 0) {   /* IPv4-mapped -> bare IPv4 */
            memmove(s_req.remote_addr, s_req.remote_addr + 7, strlen(s_req.remote_addr + 7) + 1);
        }
    } else {
        struct sockaddr_in *s4 = (struct sockaddr_in *) &sa;
        inet_ntop(AF_INET, &s4->sin_addr, s_req.remote_addr, sizeof s_req.remote_addr);
        snprintf(s_req.remote_port, sizeof s_req.remote_port, "%u", ntohs(s4->sin_port));
    }
}

/* Split the request-target into path (s_req.path) and query string (s_req.query, pointing into
 * s_req.uri). Runs on the httpd task right after the URI is read, so both the static-file check and
 * web_prepare_request can use the parts. */
static void web_split_uri(void)
{
    const char *qm = strchr(s_req.uri, '?');
    if (qm) {
        size_t pl = (size_t) (qm - s_req.uri);
        if (pl >= sizeof s_req.path) {
            pl = sizeof s_req.path - 1;
        }
        memcpy(s_req.path, s_req.uri, pl);
        s_req.path[pl] = '\0';
        s_req.query = (char *) qm + 1;
    } else {
        snprintf(s_req.path, sizeof s_req.path, "%s", s_req.uri);
        s_req.query = (char *) "";
    }
}

/* Content-Type for a static file, by extension. */
static const char *web_mime(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return "application/octet-stream";
    }
    dot++;
    if (!strcasecmp(dot, "html") || !strcasecmp(dot, "htm")) return "text/html; charset=UTF-8";
    if (!strcasecmp(dot, "txt"))                             return "text/plain; charset=UTF-8";
    if (!strcasecmp(dot, "css"))                             return "text/css";
    if (!strcasecmp(dot, "js")  || !strcasecmp(dot, "mjs"))  return "application/javascript";
    if (!strcasecmp(dot, "json")|| !strcasecmp(dot, "map"))  return "application/json";
    if (!strcasecmp(dot, "xml"))                             return "application/xml";
    if (!strcasecmp(dot, "svg"))                             return "image/svg+xml";
    if (!strcasecmp(dot, "png"))                             return "image/png";
    if (!strcasecmp(dot, "jpg") || !strcasecmp(dot, "jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, "gif"))                             return "image/gif";
    if (!strcasecmp(dot, "webp"))                            return "image/webp";
    if (!strcasecmp(dot, "ico"))                             return "image/x-icon";
    if (!strcasecmp(dot, "woff"))                            return "font/woff";
    if (!strcasecmp(dot, "woff2"))                           return "font/woff2";
    if (!strcasecmp(dot, "ttf"))                             return "font/ttf";
    if (!strcasecmp(dot, "eot"))                             return "application/vnd.ms-fontobject";
    if (!strcasecmp(dot, "pdf"))                             return "application/pdf";
    if (!strcasecmp(dot, "wasm"))                            return "application/wasm";
    return "application/octet-stream";
}

/* If the request path maps to an existing static file under the document root (public/), serve it
 * straight from the httpd task and return true -- no PHP cycle, the way a web server does before
 * handing off to the front controller. A route (missing file), a directory, or a .php file returns
 * false so the front controller handles it. Runs on the httpd task. */
static bool web_try_static(httpd_req_t *req)
{
    const char *path = s_req.path;
    if (!path[0] || strcmp(path, "/") == 0) {
        return false;                         /* "/" -> front controller (Laravel welcome) */
    }
    if (strstr(path, "..")) {
        return false;                         /* no path traversal -> let PHP 404 it */
    }
    size_t plen = strlen(path);
    if (plen >= 4 && strcasecmp(path + plen - 4, ".php") == 0) {
        return false;                         /* never serve PHP source as a static file */
    }

    char cand[512];
    int n = snprintf(cand, sizeof cand, "%s%s", s_docroot, path);
    if (n <= 0 || n >= (int) sizeof cand) {
        return false;
    }

    struct stat st;
    if (stat(cand, &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;                         /* not a real file (a route, or a directory) -> PHP */
    }

    FILE *f = fopen(cand, "rb");
    if (!f) {
        return false;
    }
    char *buf = malloc(4096);
    if (!buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        return true;
    }
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, web_mime(path));
    size_t r;
    while ((r = fread(buf, 1, 4096, f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, r) != ESP_OK) {
            break;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);      /* end of chunked response */
    free(buf);
    fclose(f);
    ESP_LOGI(TAG, "static %s (%ld bytes)", path, (long) st.st_size);
    return true;
}

/* Derive the request_info fields PHP reads *before* request startup (query string for $_GET,
 * content type/length for $_POST, method, ...). Runs in php_task just before php_request_startup. */
static void web_prepare_request(void)
{
    s_req.method_str = web_method_str(s_req.method);

    if (!s_req.host[0]) {
        snprintf(s_req.host, sizeof s_req.host, "esp32");
    }
    snprintf(s_req.server_name, sizeof s_req.server_name, "%s", s_req.host);
    char *colon = strchr(s_req.server_name, ':');
    if (colon) {
        *colon = '\0';
    }

    snprintf(s_req.server_addr, sizeof s_req.server_addr, "%s",
             s_board_ip[0] ? s_board_ip : "0.0.0.0");

    SG(request_info).request_method = s_req.method_str;
    SG(request_info).request_uri    = s_req.path;
    SG(request_info).query_string   = s_req.query;
    SG(request_info).content_type   = s_req.ctype;   /* "" when no body */
    SG(request_info).content_length = (zend_long) s_req.body_len;
    SG(request_info).proto_num      = 1001;
    s_req.body_pos = 0;
}

/* httpd handler (runs in the httpd task): parse the request off the socket, hand it to php_task,
 * wait for the response, then send whatever headers/body PHP produced. */
static esp_err_t web_handle(httpd_req_t *req)
{
    s_web_req = req;
    s_req.method = req->method;
    snprintf(s_req.uri, sizeof s_req.uri, "%s", req->uri);
    web_split_uri();

    /* Serve an existing file in public/ (robots.txt, favicon.ico, css/js/images) directly, before
     * booting the framework -- exactly what a web server does with `try_files $uri /index.php`. */
    if ((req->method == HTTP_GET || req->method == HTTP_HEAD) && web_try_static(req)) {
        return ESP_OK;
    }

    web_get_hdr(req, "Host",             s_req.host,          sizeof s_req.host);
    web_get_hdr(req, "Cookie",           s_req.cookie,        sizeof s_req.cookie);
    web_get_hdr(req, "Content-Type",     s_req.ctype,         sizeof s_req.ctype);
    web_get_hdr(req, "User-Agent",       s_req.useragent,     sizeof s_req.useragent);
    web_get_hdr(req, "Accept",           s_req.accept,        sizeof s_req.accept);
    web_get_hdr(req, "Accept-Language",  s_req.accept_lang,   sizeof s_req.accept_lang);
    web_get_hdr(req, "Referer",          s_req.referer,       sizeof s_req.referer);
    web_get_hdr(req, "X-Requested-With", s_req.xrw,           sizeof s_req.xrw);
    web_get_hdr(req, "Authorization",    s_req.authorization, sizeof s_req.authorization);
    web_peer(req);

    /* Read the POST body (if any) here, on the httpd task that owns the socket. */
    s_req.body = NULL;
    s_req.body_len = 0;
    s_req.body_pos = 0;
    if (req->content_len > 0) {
        s_req.body = malloc(req->content_len);
        if (s_req.body) {
            size_t got = 0;
            while (got < req->content_len) {
                int r = httpd_req_recv(req, s_req.body + got, req->content_len - got);
                if (r <= 0) {
                    break;
                }
                got += (size_t) r;
            }
            s_req.body_len = got;
        }
    }

    s_web_headers_done = false;
    xSemaphoreGive(s_web_req_ready);                    /* wake php_task */
    xSemaphoreTake(s_web_resp_ready, portMAX_DELAY);    /* wait until it has run the script */

    if (!s_web_headers_done) {   /* script produced no headers (e.g. a startup failure) */
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_set_type(req, "text/html; charset=UTF-8");
    }
    esp_err_t e = httpd_resp_send(req, s_web_len ? s_web_out : "", s_web_len);
    free(s_req.body);
    s_req.body = NULL;
    return e;
}

/* Run one PHP request cycle for the parked request (runs in php_task). */
static void web_serve_one(void)
{
    s_web_len = 0;
    web_prepare_request();               /* set SG(request_info) before startup ($_GET/$_POST) */
    if (php_request_startup() == SUCCESS) {
        zend_try {
            run_php_file(s_web_script);   /* fresh compile+run; output -> s_web_out */
        } zend_catch {
            /* a PHP fatal: whatever was produced before it is the response */
        } zend_end_try();
        /* Make sure the status/headers the script set reach the client even if it emitted no body
         * (a bare redirect, a 204, ...): sapi_send_headers() is a no-op once headers were sent. */
        if (!SG(headers_sent)) {
            sapi_send_headers();
        }
        php_request_shutdown(NULL);
    }
}

/* Start the HTTP server, then loop in php_task serving one request at a time. php_embed_init()
 * has already brought the engine up and opened one request; we close that so each HTTP request
 * owns a clean cycle. Never returns. */
void run_web_server(void)
{
    const char *script = g_entry_script;   /* published by php_task (app.h) */
    s_web_script = script;
    /* Document root = the directory the entry script lives in (public/ for Laravel) -- where static
     * files are served from and what $_SERVER['DOCUMENT_ROOT'] reports. */
    snprintf(s_docroot, sizeof s_docroot, "%s", script);
    char *sl = strrchr(s_docroot, '/');
    if (sl && sl != s_docroot) {
        *sl = '\0';
    }

    /* Resolve the one-time init script ([web-server] init, -DPHP_WEB_INIT) against the same source
     * mount as the entry, and only keep it if it is actually there. This model owns its init: main.c
     * just hands off. */
    const char *init_script = NULL;
#ifdef PHP_WEB_INIT
    static char init_path[160];
    if (g_src_dir) {
        snprintf(init_path, sizeof init_path, "%s/%s", g_src_dir, PHP_WEB_INIT);
        if (access(init_path, R_OK) == 0) {
            init_script = init_path;
        } else {
            ESP_LOGW(TAG, "web-server init '%s' configured but not found at %s", PHP_WEB_INIT, init_path);
        }
    }
#endif

    /* One-time init script: run it once, here, in the request php_embed_init() already opened -- so
     * its output goes to the console (we have not redirected output to the HTTP response yet). Its
     * effects that live below PHP (C-extension state, mem_ or store_ values) are then shared by every
     * request. A failure is logged but not fatal: the server still comes up. */
    if (init_script) {
        printf("--- web-server init: %s ---\n", init_script);
        fflush(stdout);
        SG(headers_sent) = 0;   /* let the init script use session/header ops like the run-once model */
        zend_try {
            run_php_file(init_script);
        } zend_catch {
            ESP_LOGE(TAG, "web-server init script bailed out (fatal error) -- continuing");
        } zend_end_try();
        printf("--- web-server init done ---\n");
        fflush(stdout);
    }

    /* Redirect output and wire the request/response hooks. sapi_startup() copied php_embed_module
     * into the live `sapi_module` at php_embed_init() time, so we set that copy. */
    sapi_module.ub_write                  = web_ub_write;
    sapi_module.send_headers              = web_send_headers;
    sapi_module.read_post                 = web_read_post;
    sapi_module.read_cookies              = web_read_cookies;
    sapi_module.register_server_variables = web_register_server_vars;
    php_request_shutdown(NULL);   /* end the request embed_init opened; module stays up */

    s_web_req_ready  = xSemaphoreCreateBinary();
    s_web_resp_ready = xSemaphoreCreateBinary();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;          /* the httpd task serves static files itself (FATFS I/O) */
    cfg.max_uri_handlers = 12;      /* one wildcard handler per HTTP method (below) */
    cfg.max_resp_headers = WEB_MAX_HDR;  /* a framework sets several (Cache-Control, Set-Cookie, ...) */
    cfg.max_req_hdr_len  = 2048;    /* browsers send a big header block (Cookie, User-Agent, sec-ch-*) */
    cfg.core_id = HTTPD_TASK_CORE;  /* opposite core to php_task, so httpd's FATFS I/O overlaps PHP */

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start: %s", esp_err_to_name(err));
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }   /* don't fall through to shutdown */
    }
    ESP_LOGI(TAG, "httpd pinned to core %d (php_task on core %d)", HTTPD_TASK_CORE, PHP_TASK_CORE);
    /* Route every method+path to the one handler; PHP does the real routing. */
    static const httpd_method_t methods[] = {
        HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_HEAD, HTTP_OPTIONS,
    };
    for (size_t i = 0; i < sizeof methods / sizeof methods[0]; i++) {
        httpd_uri_t u = { .uri = "/*", .method = methods[i], .handler = web_handle };
        httpd_register_uri_handler(server, &u);
    }
    ESP_LOGI(TAG, "web-server model: serving %s over HTTP on :80", script);

    for (;;) {
        xSemaphoreTake(s_web_req_ready, portMAX_DELAY);   /* a request arrived */
        web_serve_one();
        xSemaphoreGive(s_web_resp_ready);                 /* response is in s_web_out */
    }
}

#else
typedef int web_server_unused_t;   /* keep a non-empty translation unit when not built */
#endif /* PHP_PROJECT_WEB_SERVER */
