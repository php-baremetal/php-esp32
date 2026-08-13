/*
 * Application entry point.
 *
 * Mounts the microSD, brings the Zend engine up on a dedicated FreeRTOS task
 * (large stack: the PHP compiler recurses heavily and zend_bailout() relies on
 * setjmp/longjmp), and runs /sdcard/index.php.
 *
 * Two shapes of script are supported:
 *   - a plain script: it just runs top to bottom.
 *   - a setup()/loop() sketch (Arduino-style): setup() runs once, then loop($tick)
 *     is called repeatedly from C. Keeping the loop in C gives us a place for
 *     memory housekeeping and keeps a fatal error in PHP from taking the board
 *     down (we catch the bailout instead of letting it reach exit()).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>   /* mkdir (opcache file-cache dir) */

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"   /* board_mount_storage()/board_unmount_storage(), BOARD_NAME, BOARD_HAS_NETWORK */

#ifdef BOARD_HAS_NETWORK
#include "esp_netif.h"   /* after board.h: BOARD_HAS_NETWORK is defined there */
#endif

#include "php_embed.h"
#include "zend_API.h"
#include "zend_execute.h"
#include "zend_stream.h"
#include "zend_exceptions.h"
#include "php_variables.h"   /* php_register_variable / TRACK_VARS_SERVER (a minimal $_SERVER) */

static const char *TAG = "php-esp32";

/* Give a run-once (init-loop) script a minimal $_SERVER, like a plain "GET /" request, so a
 * framework front controller (Laravel's public/index.php) can capture a sane request instead of
 * guessing from empty globals. The web-server model sets its own per-request $_SERVER. */
/* The board's own IP once the link is up -- used for $_SERVER['SERVER_ADDR'] in the web-server
 * model. Set in php_task when the network comes up; empty if there's no network. */
static char s_board_ip[16] = "";

static void set_run_once_server_vars(const char *script)
{
    zend_is_auto_global_str(ZEND_STRL("_SERVER"));   /* force the JIT auto-global to materialise */
    zval *srv = &PG(http_globals)[TRACK_VARS_SERVER];
    if (Z_TYPE_P(srv) != IS_ARRAY) {
        return;
    }
    php_register_variable("REQUEST_METHOD", "GET", srv);
    php_register_variable("REQUEST_URI", "/", srv);
    php_register_variable("SCRIPT_NAME", "/index.php", srv);
    php_register_variable("PHP_SELF", "/index.php", srv);
    php_register_variable("SCRIPT_FILENAME", (char *) script, srv);
    php_register_variable("SERVER_PROTOCOL", "HTTP/1.1", srv);
    php_register_variable("SERVER_NAME", "esp32", srv);
    php_register_variable("HTTP_HOST", "esp32", srv);
    php_register_variable("SERVER_PORT", "80", srv);
    php_register_variable("REMOTE_ADDR", "127.0.0.1", srv);
    php_register_variable("SERVER_SOFTWARE", "php-esp32", srv);
}

#ifdef BOARD_HAS_NETWORK
/* Apply static DNS servers (a ","-separated list) to the default netif, overriding whatever DHCP
 * handed out. Up to two are used (lwIP keeps a main + a backup); extras and blanks are ignored.
 * An empty list is a no-op, leaving the DHCP-provided servers in place. */
static void net_apply_static_dns(const char *list)
{
    if (!list || !*list) {
        return;
    }
    esp_netif_t *netif = esp_netif_get_default_netif();
    if (!netif) {
        ESP_LOGW(TAG, "static DNS: no default netif");
        return;
    }
    char buf[128];
    strncpy(buf, list, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    int idx = 0;
    for (char *save = NULL, *tok = strtok_r(buf, ",", &save);
         tok && idx < 2;
         tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;              /* trim leading spaces */
        if (!*tok) continue;
        esp_netif_dns_info_t dns = {0};
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        if (esp_netif_str_to_ip4(tok, &dns.ip.u_addr.ip4) != ESP_OK) {
            ESP_LOGW(TAG, "static DNS: bad address '%s'", tok);
            continue;
        }
        esp_netif_dns_type_t t = (idx == 0) ? ESP_NETIF_DNS_MAIN : ESP_NETIF_DNS_BACKUP;
        if (esp_netif_set_dns_info(netif, t, &dns) == ESP_OK) {
            ESP_LOGI(TAG, "static DNS[%d] = %s", idx, tok);
            idx++;
        }
    }
}
#endif

/* 64 KB: with a smaller stack the board resets on trivial scripts.
 * ESP-IDF's xTaskCreate takes the stack size in bytes. */
#define PHP_TASK_STACK_BYTES (64 * 1024)

/* Two independent sources, mounted together when both are present:
 *   - the microSD at /sdcard: writable data (SQLite, logs, files the script writes).
 *   - the embedded PHP source at /app: a read-only FAT image in internal flash, built
 *     only when the firmware is made for "embedded" storage. index.php runs from here if
 *     present, otherwise from the card -- and an embedded project can still use the card
 *     for its data. */
#define SD_MOUNT_POINT  "/sdcard"
#define APP_MOUNT_POINT "/app"
/* The entry script within the source, from [php] entry (default index.php). A framework whose
 * front controller is nested sets it -- e.g. Laravel: PHP_ENTRY="public/index.php". flash-tool
 * passes it as -DPHP_ENTRY; the paths are compile-time string concatenations. */
#ifndef PHP_ENTRY
#define PHP_ENTRY "index.php"
#endif
#define SD_SCRIPT       SD_MOUNT_POINT "/" PHP_ENTRY
#define APP_SCRIPT      APP_MOUNT_POINT "/" PHP_ENTRY

/*
 * Output sink for the engine: echo, print, printf, var_dump and php_printf() all
 * funnel through here. Write straight to the console and always report the full
 * length. The embed SAPI's default ub_write treats a short write as a dropped
 * connection, which fires php_handle_aborted_connection() -> zend_bailout() ->
 * exit(); on this target exit() then aborts inside newlib.
 */
static size_t esp_ub_write(const char *str, size_t len)
{
    fwrite(str, 1, len, stdout);
    fflush(stdout);
    return len;
}

/* Compile and run a file (handles <?php ... ?>). Defines any functions in it. */
static void run_php_file(const char *path)
{
    zend_file_handle file_handle;
    zend_stream_init_filename(&file_handle, path);
    if (!php_execute_script(&file_handle)) {
        ESP_LOGE(TAG, "php_execute_script(%s) failed", path);
    }
    zend_destroy_file_handle(&file_handle);
}

static zend_function *find_php_function(const char *name)
{
    return zend_hash_str_find_ptr(EG(function_table), name, strlen(name));
}

/*
 * If the loaded script defined loop(), drive it Arduino-style: setup() once, then
 * loop($tick) forever. delay() inside PHP maps to vTaskDelay(), which yields the
 * core so the watchdog stays happy; a loop() that never calls delay() will trip it.
 */
static void run_setup_loop(void)
{
    zend_function *fn_loop = find_php_function("loop");
    if (!fn_loop) {
        return;   /* plain script; it already ran */
    }

    zval ret;

    zend_function *fn_setup = find_php_function("setup");
    if (fn_setup) {
        ZVAL_UNDEF(&ret);
        zend_call_known_function(fn_setup, NULL, NULL, &ret, 0, NULL, NULL);
        zval_ptr_dtor(&ret);
    }

    ESP_LOGI(TAG, "entering loop()");
    for (uint32_t tick = 0; ; tick++) {
        zval arg;
        ZVAL_LONG(&arg, tick);
        ZVAL_UNDEF(&ret);
        zend_call_known_function(fn_loop, NULL, NULL, &ret, 1, &arg, NULL);
        zval_ptr_dtor(&ret);

        if (EG(exception)) {
            zend_clear_exception();   /* an uncaught PHP exception: log-and-continue */
            ESP_LOGW(TAG, "uncaught exception in loop() at tick %u", (unsigned) tick);
        }

        /* Refcounting frees most garbage immediately; cycles need a periodic sweep,
         * or a long-running device accumulates them until it dies. Also a good spot
         * to watch that memory isn't creeping up. */
        if ((tick & 0xFF) == 0) {
            gc_collect_cycles();
            ESP_LOGI(TAG, "tick %u -- heap free: %u bytes",
                     (unsigned) tick, (unsigned) esp_get_free_heap_size());
        }
    }
}

/*
 * Per-project C extensions. A project can drop custom extensions in ./firmware/exts/<name>/;
 * the php_project_exts component compiles them and generates this table (linked with
 * WHOLE_ARCHIVE). The symbols are weak so a firmware built without any still links -- the count
 * then resolves to 0. Each entry is registered after php_embed_init(), so its functions, classes
 * and constants are available to the script (MINIT runs; there is no per-request RINIT for a
 * module added this late, which a hardware-driver extension doesn't need).
 */
extern zend_module_entry * const php_esp32_project_extensions[] __attribute__((weak));
extern const int php_esp32_project_extension_count __attribute__((weak));

static void register_project_extensions(void)
{
    if (&php_esp32_project_extension_count == NULL || php_esp32_project_extension_count == 0) {
        return;
    }
    for (int i = 0; i < php_esp32_project_extension_count; i++) {
        zend_module_entry *m = php_esp32_project_extensions[i];
        if (zend_startup_module(m) == SUCCESS) {
            ESP_LOGI(TAG, "project ext '%s' registered", m->name);
        } else {
            ESP_LOGW(TAG, "project ext '%s' failed to register", m->name);
        }
    }
}

/* Mount the optional embedded PHP source: a read-only FAT image in the internal
 * 'storage' partition. Absent on microSD-only firmware (the partition may not exist, or
 * exist but hold no image) -- in which case this returns false and we run from the SD. */
static bool s_app_mounted;

static bool mount_embedded(void)
{
    const esp_partition_t *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "storage");
    if (!p) {
        return false;
    }
    esp_vfs_fat_mount_config_t cfg = { .max_files = 5 };
    if (esp_vfs_fat_spiflash_mount_ro(APP_MOUNT_POINT, "storage", &cfg) != ESP_OK) {
        return false;
    }
    s_app_mounted = true;
    return true;
}

#ifdef PHP_PROJECT_WEB_SERVER
#include <strings.h>          /* strncasecmp */
#include <stdio.h>            /* fopen/fread (static files) */
#include <sys/stat.h>         /* stat / S_ISREG (static files) */
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
static const char *s_ws_script;
static char  *s_ws_out;              /* per-request output buffer (grows as needed) */
static size_t s_ws_len, s_ws_cap;
static httpd_req_t *s_ws_req;         /* the request php_task should serve */
static bool   s_ws_headers_done;      /* did ws_send_headers() run for this request? */
static SemaphoreHandle_t s_ws_req_ready;   /* httpd -> php_task: a request is waiting */
static SemaphoreHandle_t s_ws_resp_ready;  /* php_task -> httpd: the response is ready */

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
    size_t      body_pos;             /* consumed by ws_read_post */
} ws_request_t;
static ws_request_t s_req;

/* The document root (dirname of the entry script, e.g. /sdcard/public) -- constant for the run.
 * Computed once at startup; used for static-file serving and $_SERVER['DOCUMENT_ROOT']. */
static char s_docroot[256];

/* Captured response headers. httpd_resp_set_hdr stores the pointers we pass, and PHP frees its own
 * header strings at request shutdown, so we copy each "Field\0value" into buffers that outlive the
 * send (which happens back in the httpd task, after php_request_shutdown). */
#define WS_MAX_HDR 24
static char s_status_line[48];
static char s_ctype_hdr[192];
static char s_hdr_store[WS_MAX_HDR][320];

static const char *ws_method_str(int m)
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

static const char *ws_reason(int code)
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
static size_t ws_ub_write(const char *str, size_t len)
{
    if (s_ws_len + len > s_ws_cap) {
        size_t ncap = (s_ws_len + len) * 2 + 1024;
        char *n = realloc(s_ws_out, ncap);
        if (!n) {
            return len;   /* drop output under OOM rather than fail the write */
        }
        s_ws_out = n;
        s_ws_cap = ncap;
    }
    memcpy(s_ws_out + s_ws_len, str, len);
    s_ws_len += len;
    return len;
}

/* read_cookies hook: hand PHP the raw Cookie header so it can build $_COOKIE (sessions). */
static char *ws_read_cookies(void)
{
    return s_req.cookie[0] ? s_req.cookie : NULL;
}

/* read_post hook: feed PHP the POST body it needs for $_POST / php://input. */
static size_t ws_read_post(char *buffer, size_t count)
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
static void ws_register_server_vars(zval *arr)
{
    php_register_variable("REQUEST_METHOD", s_req.method_str, arr);
    php_register_variable("REQUEST_URI", s_req.uri, arr);
    php_register_variable("QUERY_STRING", s_req.query, arr);
    php_register_variable("SCRIPT_NAME", "/index.php", arr);
    php_register_variable("PHP_SELF", "/index.php", arr);
    php_register_variable("SCRIPT_FILENAME", (char *) s_ws_script, arr);
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
}

/* send_headers hook (runs in php_task): translate the headers/status the script set into the httpd
 * response. Copies each header so it survives request shutdown before the httpd task sends. */
static int ws_send_headers(sapi_headers_struct *h)
{
    int code = h->http_response_code ? h->http_response_code : 200;
    snprintf(s_status_line, sizeof s_status_line, "%d %s", code, ws_reason(code));
    httpd_resp_set_status(s_ws_req, s_status_line);

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
                httpd_resp_set_type(s_ws_req, s_ctype_hdr);
                have_ctype = true;
            } else if (slot < WS_MAX_HDR) {
                char *buf = s_hdr_store[slot];
                if (flen > 200) {
                    flen = 200;
                }
                memcpy(buf, line, flen);
                buf[flen] = '\0';
                char *vbuf = buf + flen + 1;
                snprintf(vbuf, sizeof s_hdr_store[slot] - flen - 1, "%s", val);
                httpd_resp_set_hdr(s_ws_req, buf, vbuf);   /* field/value both persist in buf */
                slot++;
            }
        }
        hh = zend_llist_get_next_ex(&h->headers, &pos);
    }
    if (!have_ctype) {
        httpd_resp_set_type(s_ws_req, "text/html; charset=UTF-8");
    }
    s_ws_headers_done = true;
    return SAPI_HEADER_SENT_SUCCESSFULLY;
}

/* Read one request header into a fixed buffer (empty string if absent). */
static void ws_get_hdr(httpd_req_t *req, const char *name, char *buf, size_t sz)
{
    buf[0] = '\0';
    httpd_req_get_hdr_value_str(req, name, buf, sz);   /* leaves buf "" if not found */
}

/* Fill REMOTE_ADDR / REMOTE_PORT from the peer socket. */
static void ws_peer(httpd_req_t *req)
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
 * ws_prepare_request can use the parts. */
static void ws_split_uri(void)
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
static const char *ws_mime(const char *path)
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
static bool ws_try_static(httpd_req_t *req)
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
    httpd_resp_set_type(req, ws_mime(path));
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
static void ws_prepare_request(void)
{
    s_req.method_str = ws_method_str(s_req.method);

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
static esp_err_t ws_handle(httpd_req_t *req)
{
    s_ws_req = req;
    s_req.method = req->method;
    snprintf(s_req.uri, sizeof s_req.uri, "%s", req->uri);
    ws_split_uri();

    /* Serve an existing file in public/ (robots.txt, favicon.ico, css/js/images) directly, before
     * booting the framework -- exactly what a web server does with `try_files $uri /index.php`. */
    if ((req->method == HTTP_GET || req->method == HTTP_HEAD) && ws_try_static(req)) {
        return ESP_OK;
    }

    ws_get_hdr(req, "Host",             s_req.host,          sizeof s_req.host);
    ws_get_hdr(req, "Cookie",           s_req.cookie,        sizeof s_req.cookie);
    ws_get_hdr(req, "Content-Type",     s_req.ctype,         sizeof s_req.ctype);
    ws_get_hdr(req, "User-Agent",       s_req.useragent,     sizeof s_req.useragent);
    ws_get_hdr(req, "Accept",           s_req.accept,        sizeof s_req.accept);
    ws_get_hdr(req, "Accept-Language",  s_req.accept_lang,   sizeof s_req.accept_lang);
    ws_get_hdr(req, "Referer",          s_req.referer,       sizeof s_req.referer);
    ws_get_hdr(req, "X-Requested-With", s_req.xrw,           sizeof s_req.xrw);
    ws_get_hdr(req, "Authorization",    s_req.authorization, sizeof s_req.authorization);
    ws_peer(req);

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

    s_ws_headers_done = false;
    xSemaphoreGive(s_ws_req_ready);                    /* wake php_task */
    xSemaphoreTake(s_ws_resp_ready, portMAX_DELAY);    /* wait until it has run the script */

    if (!s_ws_headers_done) {   /* script produced no headers (e.g. a startup failure) */
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_set_type(req, "text/html; charset=UTF-8");
    }
    esp_err_t e = httpd_resp_send(req, s_ws_len ? s_ws_out : "", s_ws_len);
    free(s_req.body);
    s_req.body = NULL;
    return e;
}

/* Run one PHP request cycle for the parked request (runs in php_task). */
static void ws_serve_one(void)
{
    s_ws_len = 0;
    ws_prepare_request();               /* set SG(request_info) before startup ($_GET/$_POST) */
    if (php_request_startup() == SUCCESS) {
        zend_try {
            run_php_file(s_ws_script);   /* fresh compile+run; output -> s_ws_out */
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
static void run_web_server(const char *script)
{
    s_ws_script = script;
    /* Document root = the directory the entry script lives in (public/ for Laravel) -- where static
     * files are served from and what $_SERVER['DOCUMENT_ROOT'] reports. */
    snprintf(s_docroot, sizeof s_docroot, "%s", script);
    char *sl = strrchr(s_docroot, '/');
    if (sl && sl != s_docroot) {
        *sl = '\0';
    }
    /* Redirect output and wire the request/response hooks. sapi_startup() copied php_embed_module
     * into the live `sapi_module` at php_embed_init() time, so we set that copy. */
    sapi_module.ub_write                  = ws_ub_write;
    sapi_module.send_headers              = ws_send_headers;
    sapi_module.read_post                 = ws_read_post;
    sapi_module.read_cookies              = ws_read_cookies;
    sapi_module.register_server_variables = ws_register_server_vars;
    php_request_shutdown(NULL);   /* end the request embed_init opened; module stays up */

    s_ws_req_ready  = xSemaphoreCreateBinary();
    s_ws_resp_ready = xSemaphoreCreateBinary();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;          /* the httpd task serves static files itself (FATFS I/O) */
    cfg.max_uri_handlers = 12;      /* one wildcard handler per HTTP method (below) */
    cfg.max_resp_headers = WS_MAX_HDR;  /* a framework sets several (Cache-Control, Set-Cookie, ...) */
    cfg.max_req_hdr_len  = 2048;    /* browsers send a big header block (Cookie, User-Agent, sec-ch-*) */

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start: %s", esp_err_to_name(err));
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }   /* don't fall through to shutdown */
    }
    /* Route every method+path to the one handler; PHP does the real routing. */
    static const httpd_method_t methods[] = {
        HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_HEAD, HTTP_OPTIONS,
    };
    for (size_t i = 0; i < sizeof methods / sizeof methods[0]; i++) {
        httpd_uri_t u = { .uri = "/*", .method = methods[i], .handler = ws_handle };
        httpd_register_uri_handler(server, &u);
    }
    ESP_LOGI(TAG, "web-server model: serving %s over HTTP on :80", script);

    for (;;) {
        xSemaphoreTake(s_ws_req_ready, portMAX_DELAY);   /* a request arrived */
        ws_serve_one();
        xSemaphoreGive(s_ws_resp_ready);                 /* response is in s_ws_out */
    }
}
#endif /* PHP_PROJECT_WEB_SERVER */

#ifdef PHP_EXT_OPCACHE_ENABLED
/* OPcache is compiled in (see docs/opcache.md). Its directives are PHP_INI_SYSTEM, so they can't be
 * set at runtime; we seed them here through the embed SAPI's ini_defaults hook, before startup. */
#ifndef PHP_OPCACHE_SHM_MODE
static char s_opcache_dir[96];   /* the writable file-cache dir, set once the card is mounted */
#endif

static void opc_add_ini_default(HashTable *ht, const char *name, const char *val)
{
    zval z;
    ZVAL_NEW_STR(&z, zend_string_init(val, strlen(val), 1));   /* persistent: freed by config dtor */
    zend_hash_str_update(ht, name, strlen(name), &z);
}

static void opcache_ini_defaults(HashTable *ht)
{
    opc_add_ini_default(ht, "opcache.enable",                  "1");
    opc_add_ini_default(ht, "opcache.enable_cli",             "1");   /* the embed SAPI is CLI-like */
    opc_add_ini_default(ht, "opcache.validate_timestamps",    "0");   /* code is static on the card */
    opc_add_ini_default(ht, "opcache.use_cwd",                "0");   /* all paths are absolute */
    /* No RTC/NTP: the clock sits at 1970 while the card's files are dated in the "future", so
     * OPcache's "file too new to cache" guard would skip every file. Disable it (validate_timestamps
     * is off anyway, so mtime doesn't matter). */
    opc_add_ini_default(ht, "opcache.file_update_protection", "0");
#ifdef PHP_OPCACHE_SHM_MODE
    /* In-RAM cache (opcache `in_memory` setting): the compiled bytecode stays in PSRAM (SHM backend,
     * shared_alloc_malloc.c) between requests, so after warm-up there's neither a recompile nor an
     * SD read. The catch: the whole bytecode plus the per-request heap must fit in the 32 MB PSRAM.
     * Fine for a small app; a large framework (Laravel) doesn't fit -- use the file cache for those.
     * memory_consumption is reserved up front, straight out of the per-request heap budget. */
    opc_add_ini_default(ht, "opcache.memory_consumption",      "16");   /* MB of PSRAM for the cache */
    opc_add_ini_default(ht, "opcache.interned_strings_buffer", "2");    /* MB, carved from the above */
    opc_add_ini_default(ht, "opcache.max_accelerated_files",  "4000");
    opc_add_ini_default(ht, "opcache.protect_memory",          "0");    /* mprotect is a no-op here */
#else
    /* File cache on the microSD (default): the bytecode lives on the card and is reloaded per request
     * (skipping the recompile), so the request keeps the full PSRAM. The right choice for a large
     * framework. */
    opc_add_ini_default(ht, "opcache.file_cache",          s_opcache_dir);
    opc_add_ini_default(ht, "opcache.file_cache_only",     "1");
    opc_add_ini_default(ht, "opcache.max_accelerated_files", "20000");
#endif
}
#endif /* PHP_EXT_OPCACHE_ENABLED */

static void php_task(void *arg)
{
    (void)arg;

    /* Line-buffer stdout/stderr: unbuffered streams push newlib through __sbprintf,
     * which creates a per-call lock that aborts on this target. */
    setvbuf(stdout, NULL, _IOLBF, 256);
    setvbuf(stderr, NULL, _IOLBF, 256);

    /* PHP allocations go to PSRAM (see CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0), which
     * keeps internal RAM free for DMA and FreeRTOS. */
    setenv("USE_ZEND_ALLOC", "0", 1);

#ifdef PHP_STORAGE_MICROSD
    /* microSD -- writable data storage, mounted whenever a card is present. Compiled out when
     * microSD support is off (-DPHP_STORAGE_MICROSD=OFF): a board without a card slot, or an
     * embedded project that didn't opt into the card. */
    bool have_sd = board_mount_storage(SD_MOUNT_POINT);
    if (have_sd) {
        ESP_LOGI(TAG, "microSD mounted at %s", SD_MOUNT_POINT);
    } else {
        ESP_LOGW(TAG, "microSD not mounted (no card / wrong format?)");
    }
#endif
    /* Embedded PHP source (read-only) -- only on firmware built for embedded storage. */
    bool have_app = mount_embedded();
    if (have_app) {
        ESP_LOGI(TAG, "embedded source mounted at %s", APP_MOUNT_POINT);
    }

#ifdef BOARD_HAS_NETWORK
    /* Boards with wired networking bring the link up here and log the address, so a PHP
     * script (e.g. a socket server) has the network ready and you can see where to reach
     * it. Non-fatal: without a cable/lease we just log it and carry on. */
    {
        char ip[16];
        if (board_network_up(ip, sizeof ip)) {
            snprintf(s_board_ip, sizeof s_board_ip, "%s", ip);
            ESP_LOGI(TAG, "network up -- http://%s/", ip);
        } else {
            ESP_LOGW(TAG, "network: no IP (link down or no DHCP)");
        }
    }
#ifdef PHP_NET_DNS
    /* Static DNS servers from [network] dns in the project config (","-separated, passed as
     * -DPHP_NET_DNS). Set them on the default netif *after* the DHCP lease so they take
     * precedence over DHCP-provided ones; if this list is empty the DHCP servers stand. */
    net_apply_static_dns(PHP_NET_DNS);
#endif
#endif

    /* Run the embedded source if it's there, otherwise the one on the card. */
    const char *script = NULL;
    const char *src_dir = NULL;   /* the mount that source lives on (for OPENSSL_CONF below) */
    if (have_app && access(APP_SCRIPT, R_OK) == 0) {
        script = APP_SCRIPT;
        src_dir = APP_MOUNT_POINT;
    }
#ifdef PHP_STORAGE_MICROSD
    else if (have_sd && access(SD_SCRIPT, R_OK) == 0) {
        script = SD_SCRIPT;
        src_dir = SD_MOUNT_POINT;
    }
#endif

    /* The full openssl build needs an openssl.cnf; point it at one shipped with the source
     * (see docs/openssl.md). The path is PHP_OPENSSL_CONF (set from the project config's
     * [extensions.openssl] config_path, default "openssl.cnf"): an absolute path is used as-is,
     * a relative one is resolved against the source mount. Harmless when openssl isn't built or
     * the file isn't there. */
#ifndef PHP_OPENSSL_CONF
#define PHP_OPENSSL_CONF "openssl.cnf"
#endif
    if (src_dir) {
        static char ossl_conf[128];
        if (PHP_OPENSSL_CONF[0] == '/')
            snprintf(ossl_conf, sizeof ossl_conf, "%s", PHP_OPENSSL_CONF);
        else
            snprintf(ossl_conf, sizeof ossl_conf, "%s/%s", src_dir, PHP_OPENSSL_CONF);
        setenv("OPENSSL_CONF", ossl_conf, 1);
    }

    /* The TLS client (PHP_EXT_OPENSSL_TLS) verifies peers against a CA bundle shipped with the
     * source. PHP_TLS_CAFILE is its path (from [extensions.openssl] certs_path, default
     * "certs/ca-bundle.crt"): absolute used as-is, relative resolved against the source mount. The
     * esp-tls factory reads $PHP_TLS_CAFILE; with none it connects unverified (and logs it). */
#ifdef PHP_TLS_CAFILE
    if (src_dir) {
        static char tls_ca[128];
        if (PHP_TLS_CAFILE[0] == '/')
            snprintf(tls_ca, sizeof tls_ca, "%s", PHP_TLS_CAFILE);
        else
            snprintf(tls_ca, sizeof tls_ca, "%s/%s", src_dir, PHP_TLS_CAFILE);
        setenv("PHP_TLS_CAFILE", tls_ca, 1);
    }
#endif

    php_embed_module.ub_write = esp_ub_write;

#ifdef PHP_PROJECT_WEB_SERVER
    /* Present a web-like SAPI name to the script. The embed SAPI is named "embed", which frameworks
     * treat as a CLI process -- Symfony then picks its CLI error renderer (which writes to
     * php://stdout, unavailable here) and Laravel's runningInConsole() returns true. In the
     * web-server model each run really is an HTTP request, so report "cli-server" (PHP's built-in
     * web server), which frameworks treat as web. Must be set before php_embed_init() so PHP_SAPI
     * reflects it. */
    php_embed_module.name = "cli-server";
#endif

#ifdef PHP_EXT_OPCACHE_ENABLED
    /* Enable OPcache: install the ini_defaults hook (must be set before php_embed_init reads the
     * ini). The mode is chosen at build time by the `in_memory` setting. */
#ifdef PHP_OPCACHE_SHM_MODE
    php_embed_module.ini_defaults = opcache_ini_defaults;   /* in-RAM (PSRAM) -- no card needed */
    ESP_LOGI(TAG, "opcache: in-RAM (PSRAM SHM) bytecode cache");
#elif defined(PHP_STORAGE_MICROSD)
    /* File cache: point it at a writable dir on the card. */
    if (have_sd) {
        snprintf(s_opcache_dir, sizeof s_opcache_dir, "%s/opcache", SD_MOUNT_POINT);
        mkdir(s_opcache_dir, 0777);   /* opcache requires the dir to exist; ok if it already does */
        php_embed_module.ini_defaults = opcache_ini_defaults;
        ESP_LOGI(TAG, "opcache: file cache at %s", s_opcache_dir);
    } else {
        ESP_LOGW(TAG, "opcache: no microSD, not enabled (needs a writable cache dir)");
    }
#endif
#endif

    ESP_LOGI(TAG, "php_embed_init()...");
    if (php_embed_init(0, NULL) != SUCCESS) {
        ESP_LOGE(TAG, "php_embed_init failed");
        vTaskDelete(NULL);
        return;
    }

    /* Register any per-project C extensions (from ./firmware/exts) before the script runs. */
    register_project_extensions();

    /* Firmware banners go straight to stdout, NOT through php_printf: php_printf runs PHP's output
     * layer, which marks SG(headers_sent), and that breaks anything the script does before its own
     * first output -- e.g. session_start() / ini_set('session...') refuse once headers are "sent".
     * esp_ub_write also just writes stdout, so the console output is identical either way. */
    printf("PHP %s on %s\n", PHP_VERSION, BOARD_SOC);
    fflush(stdout);

    if (script) {
#ifdef PHP_PROJECT_WEB_SERVER
        /* web-server model: hand the script to the HTTP server, which runs it per request.
         * Never returns. */
        run_web_server(script);
#else
        printf("--- %s ---\n", script);
        fflush(stdout);
        /* The embed SAPI marks headers as already sent at init (it's a CLI-like, no-HTTP SAPI).
         * In this run-once model there are no HTTP headers, so clear it before the script: otherwise
         * session_start()/setcookie()/header() and the session ini settings all refuse with "headers
         * already sent". Header ops stay no-ops (SG(request_info).no_headers), so this only lifts the
         * false warning. (The web-server model above keeps the flag -- it sends real headers.) */
        SG(headers_sent) = 0;
        set_run_once_server_vars(script);   /* a sane GET / $_SERVER for a framework front controller */
        /* Catch a PHP bailout (fatal error / die) so it doesn't reach exit(). */
        zend_try {
            run_php_file(script);   /* runs top-level, defines setup()/loop() */
            run_setup_loop();       /* never returns if loop() is defined */
        } zend_catch {
            ESP_LOGE(TAG, "PHP bailed out (fatal error)");
        } zend_end_try();
        printf("--- end ---\n");
        fflush(stdout);
#endif
    } else {
        printf("no index.php (embedded or microSD); engine check: ");
        fflush(stdout);
        zend_eval_string("echo 1+1;", NULL, "boot");
        printf("\n");
        fflush(stdout);
    }

    php_embed_shutdown();

    if (s_app_mounted) {
        esp_vfs_fat_spiflash_unmount_ro(APP_MOUNT_POINT, "storage");
    }
#ifdef PHP_STORAGE_MICROSD
    if (have_sd) {
        board_unmount_storage(SD_MOUNT_POINT);
    }
#endif

    ESP_LOGI(TAG, "done -- heap free: %u bytes", (unsigned) esp_get_free_heap_size());
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "starting PHP runtime");
    xTaskCreate(php_task, "php", PHP_TASK_STACK_BYTES, NULL, 5, NULL);
}
