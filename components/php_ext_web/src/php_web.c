/*
 * Baremetal\Http\Request/Response/Message + serve_http/serve_ws/ws_broadcast. The httpd task parks a
 * request and posts it to the reactor; the reactor (php_task) answers and the httpd task sends the reply.
 */
#ifdef PHP_WEB_BUILD

#include "php_web.h"
#include "event_bus_php.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"
#include "SAPI.h"             /* SG(), php_default_treat_data */
#include "php_variables.h"    /* PARSE_STRING */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"   /* getpeername (the client IP) */
#include "lwip/inet.h"      /* inet_ntop */

static const char *TAG = "php-esp32";

static zend_class_entry *web_request_ce;
static zend_class_entry *web_response_ce;
static zend_class_entry *web_message_ce;   /* Baremetal\Http\Message -- an inbound WebSocket frame */

static httpd_handle_t    s_httpd;          /* the one HTTP server, shared by serve_http / serve_ws */

/* ---- Response construction (C-side) --------------------------------------------------------------- */

static void web_response_set(zval *out, zend_long status, zend_string *body, const char *ctype)
{
    object_init_ex(out, web_response_ce);
    zend_update_property_long(web_response_ce, Z_OBJ_P(out), "status", sizeof("status") - 1, status);
    zend_update_property_str(web_response_ce, Z_OBJ_P(out), "body", sizeof("body") - 1, body);
    zend_update_property_string(web_response_ce, Z_OBJ_P(out), "contentType",
                                sizeof("contentType") - 1, ctype);
}

static zend_string *web_json_encode(zval *data)
{
    zval fn, ret;
    ZVAL_STRING(&fn, "json_encode");
    zend_string *s = NULL;
    if (call_user_function(NULL, NULL, &fn, &ret, 1, data) == SUCCESS && Z_TYPE(ret) == IS_STRING) {
        s = zend_string_copy(Z_STR(ret));
    }
    zval_ptr_dtor(&fn);
    zval_ptr_dtor(&ret);
    return s ? s : ZSTR_EMPTY_ALLOC();
}

/* ---- Baremetal\Http\Response --------------------------------------------------------------------- */

ZEND_BEGIN_ARG_INFO_EX(arginfo_resp_ctor, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, status, IS_LONG, 0, "200")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, body, IS_STRING, 0, "\"\"")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, contentType, IS_STRING, 0, "\"text/html; charset=UTF-8\"")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_resp_body, 0, 0, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, body, IS_STRING, 0, "\"\"")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_resp_json, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

PHP_METHOD(Response, __construct)
{
    zend_long status = 200;
    zend_string *body = NULL, *ctype = NULL;
    ZEND_PARSE_PARAMETERS_START(0, 3)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(status)
        Z_PARAM_STR(body)
        Z_PARAM_STR(ctype)
    ZEND_PARSE_PARAMETERS_END();

    zend_update_property_long(web_response_ce, Z_OBJ_P(ZEND_THIS), "status", sizeof("status") - 1, status);
    if (body) {
        zend_update_property_str(web_response_ce, Z_OBJ_P(ZEND_THIS), "body", sizeof("body") - 1, body);
    }
    if (ctype) {
        zend_update_property_str(web_response_ce, Z_OBJ_P(ZEND_THIS), "contentType",
                                 sizeof("contentType") - 1, ctype);
    }
}

PHP_METHOD(Response, ok)
{
    zend_string *body = NULL;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(body)
    ZEND_PARSE_PARAMETERS_END();
    web_response_set(return_value, 200, body ? body : ZSTR_EMPTY_ALLOC(), "text/html; charset=UTF-8");
}

PHP_METHOD(Response, text)
{
    zend_string *body;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(body)
    ZEND_PARSE_PARAMETERS_END();
    web_response_set(return_value, 200, body, "text/plain; charset=UTF-8");
}

PHP_METHOD(Response, html)
{
    zend_string *body;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(body)
    ZEND_PARSE_PARAMETERS_END();
    web_response_set(return_value, 200, body, "text/html; charset=UTF-8");
}

PHP_METHOD(Response, json)
{
    zval *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(data)
    ZEND_PARSE_PARAMETERS_END();
    zend_string *body = web_json_encode(data);
    web_response_set(return_value, 200, body, "application/json");
    zend_string_release(body);
}

PHP_METHOD(Response, notFound)
{
    zend_string *body = NULL;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(body)
    ZEND_PARSE_PARAMETERS_END();
    if (body) {
        web_response_set(return_value, 404, body, "text/plain; charset=UTF-8");
    } else {
        zend_string *d = zend_string_init("Not Found", 9, 0);
        web_response_set(return_value, 404, d, "text/plain; charset=UTF-8");
        zend_string_release(d);
    }
}

PHP_METHOD(Response, noContent)
{
    ZEND_PARSE_PARAMETERS_NONE();
    web_response_set(return_value, 204, ZSTR_EMPTY_ALLOC(), "text/plain; charset=UTF-8");
}

static const zend_function_entry response_methods[] = {
    PHP_ME(Response, __construct, arginfo_resp_ctor, ZEND_ACC_PUBLIC)
    PHP_ME(Response, ok,        arginfo_resp_body, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Response, text,      arginfo_resp_body, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Response, html,      arginfo_resp_body, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Response, json,      arginfo_resp_json, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Response, notFound,  arginfo_resp_body, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Response, noContent, NULL,              ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

/* ---- Baremetal\Http\Request ---------------------------------------------------------------------- */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_req_json, 0, 0, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_req_header, 0, 1, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* Request::json(): json_decode($this->body, true) -- the request body as an associative value. */
PHP_METHOD(Request, json)
{
    ZEND_PARSE_PARAMETERS_NONE();
    zval rv;
    zval *body = zend_read_property(web_request_ce, Z_OBJ_P(ZEND_THIS), "body", sizeof("body") - 1, 0, &rv);
    if (!body || Z_TYPE_P(body) != IS_STRING) {
        RETURN_NULL();
    }
    zval fn, args[2];
    ZVAL_STRING(&fn, "json_decode");
    ZVAL_STR_COPY(&args[0], Z_STR_P(body));
    ZVAL_TRUE(&args[1]);
    call_user_function(NULL, NULL, &fn, return_value, 2, args);
    zval_ptr_dtor(&fn);
    zval_ptr_dtor(&args[0]);
}

/* Request::header($name): a request header by (case-insensitive) name, or null. */
PHP_METHOD(Request, header)
{
    zend_string *name;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    zval rv;
    zval *hdrs = zend_read_property(web_request_ce, Z_OBJ_P(ZEND_THIS), "headers",
                                    sizeof("headers") - 1, 0, &rv);
    if (!hdrs || Z_TYPE_P(hdrs) != IS_ARRAY) {
        RETURN_NULL();
    }
    zend_string *key;
    zval *val;
    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(hdrs), key, val) {
        if (key && zend_string_equals_ci(key, name)) {
            RETURN_COPY(val);
        }
    } ZEND_HASH_FOREACH_END();
    RETURN_NULL();
}

static const zend_function_entry request_methods[] = {
    PHP_ME(Request, json,   arginfo_req_json,   ZEND_ACC_PUBLIC)
    PHP_ME(Request, header, arginfo_req_header, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* ---- The request/response rendezvous ------------------------------------------------------------- */

#define WEB_MAX_HDRS 10
#define WS_POST_WAIT_MS 2000   /* how long the httpd task waits for a reactor-queue slot before dropping */

typedef struct {
    /* in (filled by the httpd task) */
    const char *method;
    char        path[256];
    char        query[256];
    char        ip[48];
    struct { const char *name; char value[256]; } hdr[WEB_MAX_HDRS];
    int         n_hdr;
    char       *body;            /* malloc'd, or NULL */
    size_t      body_len;
    /* out (filled by the reactor) */
    int         status;
    char       *resp_body;       /* malloc'd copy of the PHP body, freed by the httpd task */
    size_t      resp_len;
    char        resp_ctype[128];
} web_slot_t;

static web_slot_t        s_slot;
static SemaphoreHandle_t s_slot_guard;   /* one request through the slot at a time */
static SemaphoreHandle_t s_slot_resp;    /* reactor -> httpd: the response is ready */
static bool              s_started;

/* The client's IP for the parked request's socket, into buf ("" if unavailable). */
static void web_peer_ip(httpd_req_t *req, char *buf, size_t sz)
{
    buf[0] = '\0';
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
        inet_ntop(AF_INET6, &sa.sin6_addr, buf, sz);
        if (strncasecmp(buf, "::ffff:", 7) == 0) {      /* IPv4-mapped -> bare IPv4 (lwip prints FFFF) */
            memmove(buf, buf + 7, strlen(buf + 7) + 1);
        }
    } else {
        struct sockaddr_in *s4 = (struct sockaddr_in *) &sa;
        inet_ntop(AF_INET, &s4->sin_addr, buf, sz);
    }
}

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
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Status";
    }
}

/* Copy Response object fields into the slot for the httpd task to send. */
static void web_take_response(zend_object *resp)
{
    zval rv;
    zval *st = zend_read_property(web_response_ce, resp, "status", sizeof("status") - 1, 1, &rv);
    s_slot.status = st ? (int) zval_get_long(st) : 200;

    zval *ct = zend_read_property(web_response_ce, resp, "contentType", sizeof("contentType") - 1, 1, &rv);
    if (ct && Z_TYPE_P(ct) == IS_STRING) {
        snprintf(s_slot.resp_ctype, sizeof s_slot.resp_ctype, "%s", Z_STRVAL_P(ct));
    } else {
        snprintf(s_slot.resp_ctype, sizeof s_slot.resp_ctype, "text/html; charset=UTF-8");
    }

    zval *bd = zend_read_property(web_response_ce, resp, "body", sizeof("body") - 1, 1, &rv);
    if (bd && Z_TYPE_P(bd) == IS_STRING && Z_STRLEN_P(bd) > 0) {
        s_slot.resp_len = Z_STRLEN_P(bd);
        s_slot.resp_body = malloc(s_slot.resp_len);
        if (s_slot.resp_body) {
            memcpy(s_slot.resp_body, Z_STRVAL_P(bd), s_slot.resp_len);
        } else {
            s_slot.resp_len = 0;
        }
    }
}

static void web_set_plain(int status, const char *body)
{
    s_slot.status = status;
    snprintf(s_slot.resp_ctype, sizeof s_slot.resp_ctype, "text/plain; charset=UTF-8");
    size_t n = strlen(body);
    s_slot.resp_body = malloc(n);
    if (s_slot.resp_body) {
        memcpy(s_slot.resp_body, body, n);
        s_slot.resp_len = n;
    }
}

/* Parse a "a=1&b=2&x[]=..." string into dst (same rules as parse_str / $_GET). */
static void web_parse_qs(zval *dst, const char *s, size_t len)
{
    array_init(dst);
    if (!s || len == 0) {
        return;
    }
    char *dup = estrndup(s, len);
    php_default_treat_data(PARSE_STRING, dup, dst);   /* takes and frees dup */
}

/* Set a superglobal, activating the auto-global first so the engine won't rebuild it on access. */
static void web_put_global(const char *name, size_t nlen, zval *arr)
{
    zend_is_auto_global_str((char *) name, nlen);
    zend_hash_str_update(&EG(symbol_table), name, nlen, arr);
}

/* Populate $_GET/$_POST/$_REQUEST/$_SERVER for this request (the engine is resident, no SAPI cycle). */
static void web_populate_superglobals(web_slot_t *s)
{
    zval get, post, request, server;

    web_parse_qs(&get, s->query, strlen(s->query));

    bool is_form = false;
    for (int i = 0; i < s->n_hdr; i++) {
        if (strcasecmp(s->hdr[i].name, "Content-Type") == 0 &&
            strstr(s->hdr[i].value, "application/x-www-form-urlencoded")) {
            is_form = true;
        }
    }
    if (is_form && s->body && s->body_len) {
        web_parse_qs(&post, s->body, s->body_len);
    } else {
        array_init(&post);
    }

    array_init(&request);                              /* GET then POST, POST wins (request_order GP) */
    zend_hash_merge(Z_ARRVAL(request), Z_ARRVAL(get),  zval_add_ref, 1);
    zend_hash_merge(Z_ARRVAL(request), Z_ARRVAL(post), zval_add_ref, 1);

    array_init(&server);
    add_assoc_string(&server, "REQUEST_METHOD", (char *) s->method);
    char uri[512];
    if (s->query[0]) {
        snprintf(uri, sizeof uri, "%s?%s", s->path, s->query);
    } else {
        snprintf(uri, sizeof uri, "%s", s->path);
    }
    add_assoc_string(&server, "REQUEST_URI", uri);
    add_assoc_string(&server, "QUERY_STRING", s->query);
    add_assoc_string(&server, "REMOTE_ADDR", s->ip);
    add_assoc_string(&server, "SERVER_SOFTWARE", "php-esp32");
    add_assoc_string(&server, "SERVER_PROTOCOL", "HTTP/1.1");
    if (s->body_len) {
        add_assoc_long(&server, "CONTENT_LENGTH", (zend_long) s->body_len);
    }
    for (int i = 0; i < s->n_hdr; i++) {
        if (strcasecmp(s->hdr[i].name, "Content-Type") == 0) {
            add_assoc_string(&server, "CONTENT_TYPE", s->hdr[i].value);
            continue;
        }
        char key[80] = "HTTP_";                        /* Header-Name -> HTTP_HEADER_NAME */
        size_t k = 5;
        for (const char *p = s->hdr[i].name; *p && k < sizeof key - 1; p++) {
            char c = *p;
            key[k++] = (c == '-') ? '_' : (char) toupper((unsigned char) c);
        }
        key[k] = '\0';
        add_assoc_string(&server, key, s->hdr[i].value);
    }

    web_put_global("_GET", sizeof("_GET") - 1, &get);
    web_put_global("_POST", sizeof("_POST") - 1, &post);
    web_put_global("_REQUEST", sizeof("_REQUEST") - 1, &request);
    web_put_global("_SERVER", sizeof("_SERVER") - 1, &server);
}

/* Runs on the reactor (php_task, core 0): build the Request, dispatch, take the Response. */
static void web_deliver(void *handle)
{
    web_slot_t *s = handle;

    zval req;
    object_init_ex(&req, web_request_ce);
    zend_update_property_string(web_request_ce, Z_OBJ(req), "method", sizeof("method") - 1, s->method);
    zend_update_property_string(web_request_ce, Z_OBJ(req), "path", sizeof("path") - 1, s->path);
    zend_update_property_string(web_request_ce, Z_OBJ(req), "query", sizeof("query") - 1, s->query);
    zend_update_property_string(web_request_ce, Z_OBJ(req), "ip", sizeof("ip") - 1, s->ip);
    zend_update_property_stringl(web_request_ce, Z_OBJ(req), "body", sizeof("body") - 1,
                                 s->body ? s->body : "", s->body_len);
    zval hdrs;
    array_init(&hdrs);
    for (int i = 0; i < s->n_hdr; i++) {
        add_assoc_string(&hdrs, s->hdr[i].name, s->hdr[i].value);
    }
    zend_update_property(web_request_ce, Z_OBJ(req), "headers", sizeof("headers") - 1, &hdrs);
    zval_ptr_dtor(&hdrs);

    s->status = 0;
    s->resp_body = NULL;
    s->resp_len = 0;
    s->resp_ctype[0] = '\0';

    /* zend_try: a fatal in a handler still yields 500 and releases the socket, never hangs the httpd task */
    zend_try {
        web_populate_superglobals(s);
        zval resp;
        int rc = bm_events_deliver_answering(&req, &resp);
        if (rc == 1) {
            if (Z_TYPE(resp) == IS_OBJECT && instanceof_function(Z_OBJCE(resp), web_response_ce)) {
                web_take_response(Z_OBJ(resp));
            } else {
                web_set_plain(500, "handler did not return a Response");
            }
            zval_ptr_dtor(&resp);
        } else if (rc == -1) {
            zend_clear_exception();
            web_set_plain(500, "Internal Server Error");
        } else {
            web_set_plain(404, "Not Found");
        }
    } zend_catch {
        if (!s->resp_body) {
            web_set_plain(500, "Internal Server Error");
        }
    } zend_end_try();

    zval_ptr_dtor(&req);
    xSemaphoreGive(s_slot_resp);
}

/* Runs on the httpd task: parse the request, hand it to the reactor, send the response it returns. */
static esp_err_t web_handle(httpd_req_t *req)
{
    xSemaphoreTake(s_slot_guard, portMAX_DELAY);

    s_slot.method = web_method_str(req->method);

    const char *qm = strchr(req->uri, '?');
    if (qm) {
        size_t pl = (size_t) (qm - req->uri);
        if (pl >= sizeof s_slot.path) {
            pl = sizeof s_slot.path - 1;
        }
        memcpy(s_slot.path, req->uri, pl);
        s_slot.path[pl] = '\0';
        snprintf(s_slot.query, sizeof s_slot.query, "%s", qm + 1);
    } else {
        snprintf(s_slot.path, sizeof s_slot.path, "%s", req->uri);
        s_slot.query[0] = '\0';
    }

    static const char *want[] = {
        "Host", "Content-Type", "User-Agent", "Accept", "Cookie", "Authorization",
        "X-Requested-With", "Accept-Language",
    };
    s_slot.n_hdr = 0;
    for (size_t i = 0; i < sizeof want / sizeof want[0] && s_slot.n_hdr < WEB_MAX_HDRS; i++) {
        char buf[256];
        if (httpd_req_get_hdr_value_str(req, want[i], buf, sizeof buf) == ESP_OK) {
            s_slot.hdr[s_slot.n_hdr].name = want[i];
            snprintf(s_slot.hdr[s_slot.n_hdr].value, sizeof s_slot.hdr[s_slot.n_hdr].value, "%s", buf);
            s_slot.n_hdr++;
        }
    }
    web_peer_ip(req, s_slot.ip, sizeof s_slot.ip);

    s_slot.body = NULL;
    s_slot.body_len = 0;
    if (req->content_len > 0) {
        s_slot.body = malloc(req->content_len);
        if (s_slot.body) {
            size_t got = 0;
            while (got < req->content_len) {
                int r = httpd_req_recv(req, s_slot.body + got, req->content_len - got);
                if (r <= 0) {
                    break;
                }
                got += (size_t) r;
            }
            s_slot.body_len = got;
        }
    }

    esp_err_t e;
    if (!bm_events_post_http(&s_slot)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "reactor queue full");
        e = ESP_OK;
    } else {
        xSemaphoreTake(s_slot_resp, portMAX_DELAY);
        char status_line[48];
        snprintf(status_line, sizeof status_line, "%d %s", s_slot.status, web_reason(s_slot.status));
        httpd_resp_set_status(req, status_line);
        httpd_resp_set_type(req, s_slot.resp_ctype[0] ? s_slot.resp_ctype : "text/html; charset=UTF-8");
        e = httpd_resp_send(req, s_slot.resp_body ? s_slot.resp_body : "", s_slot.resp_len);
    }

    free(s_slot.resp_body);
    s_slot.resp_body = NULL;
    free(s_slot.body);
    s_slot.body = NULL;
    xSemaphoreGive(s_slot_guard);
    return e;
}

/* ---- the WebSocket source --------------------------------------------------------------------- */

/* One inbound WS frame, malloc'd for the reactor and freed after delivery. */
typedef struct {
    int    fd;            /* client socket, for reply() */
    bool   binary;
    size_t len;
    char   data[];        /* NUL-terminated payload */
} ws_msg_t;

/* Runs on the reactor (php_task, core 0): build a Message and deliver it to the listeners. */
static void web_ws_deliver(void *handle)
{
    ws_msg_t *m = handle;
    zval ev;
    object_init_ex(&ev, web_message_ce);
    zend_update_property_stringl(web_message_ce, Z_OBJ(ev), "text", sizeof("text") - 1, m->data, m->len);
    zend_update_property_long(web_message_ce, Z_OBJ(ev), "client", sizeof("client") - 1, m->fd);
    zend_update_property_bool(web_message_ce, Z_OBJ(ev), "binary", sizeof("binary") - 1, m->binary);
    zend_try {
        bm_events_deliver_object(&ev);
        if (EG(exception)) {
            zend_clear_exception();
        }
    } zend_catch {
        /* swallow: a WS handler's fatal must not take the reactor down */
    } zend_end_try();
    zval_ptr_dtor(&ev);
    free(m);
}

/* httpd task: read a frame and hand it to the reactor (a GET is the handshake -> just return OK). */
static esp_err_t web_ws_handle(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }
    httpd_ws_frame_t frame = { 0 };
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t r = httpd_ws_recv_frame(req, &frame, 0);   /* length only */
    if (r != ESP_OK) {
        return r;
    }
    if (frame.len == 0) {
        return ESP_OK;
    }
    ws_msg_t *m = malloc(sizeof(ws_msg_t) + frame.len + 1);
    if (!m) {
        return ESP_ERR_NO_MEM;
    }
    frame.payload = (uint8_t *) m->data;
    r = httpd_ws_recv_frame(req, &frame, frame.len);
    if (r != ESP_OK) {
        free(m);
        return r;
    }
    m->data[frame.len] = '\0';
    m->len = frame.len;
    m->binary = (frame.type == HTTPD_WS_TYPE_BINARY);
    m->fd = httpd_req_to_sockfd(req);
    /* backpressure: block until the reactor has room (TCP slows the sender); drop only if it times out */
    if (!bm_events_post_ws(m, WS_POST_WAIT_MS)) {
        free(m);
    }
    return ESP_OK;
}

/* ---- serve_http() / serve_ws() ---------------------------------------------------------------------- */

/* Start the one shared HTTP server on `port` if it is not already up. */
static bool web_ensure_server(uint16_t port)
{
    if (s_httpd) {
        return true;
    }
    bm_events_ensure_queue();
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = port;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 12;
    cfg.max_req_hdr_len = 2048;
    cfg.send_wait_timeout = 2;   /* bound a stuck outbound send (default 10 s) -- a slow client is dropped */
    return httpd_start(&s_httpd, &cfg) == ESP_OK;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_serve_http, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, port, IS_LONG, 0, "80")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_serve_ws, 0, 0, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, path, IS_STRING, 0, "\"/ws\"")
ZEND_END_ARG_INFO()

/* serve_http(int $port = 80): bool -- start the HTTP server; requests arrive as Request events. */
PHP_FUNCTION(serve_http)
{
    zend_long port = 80;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(port)
    ZEND_PARSE_PARAMETERS_END();

    if (s_started) {
        RETURN_TRUE;
    }
    if (!web_ensure_server((uint16_t) port)) {
        zend_throw_exception(zend_ce_exception, "serve_http(): httpd_start failed", 0);
        RETURN_THROWS();
    }
    bm_events_set_http_handler(web_deliver);
    s_slot_guard = xSemaphoreCreateMutex();
    s_slot_resp  = xSemaphoreCreateBinary();
    if (!s_slot_guard || !s_slot_resp) {
        zend_throw_exception(zend_ce_exception, "serve_http(): out of memory", 0);
        RETURN_THROWS();
    }
    static const httpd_method_t methods[] = {
        HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_HEAD, HTTP_OPTIONS,
    };
    for (size_t i = 0; i < sizeof methods / sizeof methods[0]; i++) {
        httpd_uri_t u = { .uri = "/*", .method = methods[i], .handler = web_handle };
        httpd_register_uri_handler(s_httpd, &u);   /* registered after any serve_ws() path */
    }
    s_started = true;
    ESP_LOGI(TAG, "serve_http: listening on :%ld (requests -> Request events)", (long) port);
    RETURN_TRUE;
}

/* serve_ws(string $path = "/ws"): bool -- WebSocket endpoint; inbound frames arrive as Message events. */
PHP_FUNCTION(serve_ws)
{
    zend_string *path = NULL;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(path)
    ZEND_PARSE_PARAMETERS_END();
    const char *p = path ? ZSTR_VAL(path) : "/ws";

    if (!web_ensure_server(80)) {
        zend_throw_exception(zend_ce_exception, "serve_ws(): httpd_start failed", 0);
        RETURN_THROWS();
    }
    bm_events_set_ws_handler(web_ws_deliver);

    httpd_uri_t u = { .uri = p, .method = HTTP_GET, .handler = web_ws_handle, .is_websocket = true };
    if (httpd_register_uri_handler(s_httpd, &u) != ESP_OK) {
        zend_throw_exception(zend_ce_exception, "serve_ws(): cannot register the WS handler", 0);
        RETURN_THROWS();
    }
    /* move the http catch-all after this path (re-register) so the exact WS route matches first */
    if (s_started) {
        httpd_unregister_uri_handler(s_httpd, "/*", HTTP_GET);
        httpd_uri_t g = { .uri = "/*", .method = HTTP_GET, .handler = web_handle };
        httpd_register_uri_handler(s_httpd, &g);
    }
    ESP_LOGI(TAG, "serve_ws: WebSocket endpoint at %s (frames -> Message events)", p);
    RETURN_TRUE;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ws_reply, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* Message::reply(string $data): bool -- send a text frame back to the client this message came from. */
PHP_METHOD(Message, reply)
{
    zend_string *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(data)
    ZEND_PARSE_PARAMETERS_END();

    zval rv;
    zval *fd = zend_read_property(web_message_ce, Z_OBJ_P(ZEND_THIS), "client", sizeof("client") - 1, 0, &rv);
    if (!s_httpd || !fd) {
        RETURN_FALSE;
    }
    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *) ZSTR_VAL(data),
        .len = ZSTR_LEN(data),
    };
    RETURN_BOOL(httpd_ws_send_frame_async(s_httpd, (int) zval_get_long(fd), &frame) == ESP_OK);
}

static const zend_function_entry message_methods[] = {
    PHP_ME(Message, reply, arginfo_ws_reply, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_ws_broadcast, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* ws_broadcast(string $data): int -- send a text frame to every WS client, returns how many reached.
 * Non-blocking: a zero-timeout select() skips a client whose buffer is full, so a slow one can't stall
 * the reactor. */
PHP_FUNCTION(ws_broadcast)
{
    zend_string *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(data)
    ZEND_PARSE_PARAMETERS_END();

    if (!s_httpd) {
        RETURN_LONG(0);
    }
    size_t n = 16;
    int fds[16];
    if (httpd_get_client_list(s_httpd, &n, fds) != ESP_OK) {
        RETURN_LONG(0);
    }
    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *) ZSTR_VAL(data),
        .len = ZSTR_LEN(data),
    };
    zend_long sent = 0;
    for (size_t i = 0; i < n; i++) {
        int fd = fds[i];
        if (httpd_ws_get_fd_info(s_httpd, fd) != HTTPD_WS_CLIENT_WEBSOCKET) {
            continue;
        }
        fd_set wf;
        FD_ZERO(&wf);
        FD_SET(fd, &wf);
        struct timeval z = { 0, 0 };
        if (select(fd + 1, NULL, &wf, NULL, &z) <= 0 || !FD_ISSET(fd, &wf)) {
            continue;   /* not writable now -> drop this frame for this client, never block */
        }
        if (httpd_ws_send_frame_async(s_httpd, fd, &frame) == ESP_OK) {
            sent++;
        }
    }
    RETURN_LONG(sent);
}

/* ---- module ------------------------------------------------------------------------------------- */

PHP_MINIT_FUNCTION(web)
{
    zend_class_entry ce;
    zend_class_entry *event_base = bm_events_base_ce();   /* a Request is a request/response event */

    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Http", "Request", request_methods);
    web_request_ce = event_base ? zend_register_internal_class_ex(&ce, event_base)
                                : zend_register_internal_class(&ce);
    zend_declare_property_string(web_request_ce, "method", sizeof("method") - 1, "GET", ZEND_ACC_PUBLIC);
    zend_declare_property_string(web_request_ce, "path", sizeof("path") - 1, "/", ZEND_ACC_PUBLIC);
    zend_declare_property_string(web_request_ce, "query", sizeof("query") - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_string(web_request_ce, "body", sizeof("body") - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_string(web_request_ce, "ip", sizeof("ip") - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_null(web_request_ce, "headers", sizeof("headers") - 1, ZEND_ACC_PUBLIC);

    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Http", "Response", response_methods);
    web_response_ce = zend_register_internal_class(&ce);
    zend_declare_property_long(web_response_ce, "status", sizeof("status") - 1, 200, ZEND_ACC_PUBLIC);
    zend_declare_property_string(web_response_ce, "body", sizeof("body") - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_string(web_response_ce, "contentType", sizeof("contentType") - 1,
                                 "text/html; charset=UTF-8", ZEND_ACC_PUBLIC);

    /* Baremetal\Http\Message -- an inbound WebSocket frame (a fire-and-forget event). */
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Http", "Message", message_methods);
    web_message_ce = event_base ? zend_register_internal_class_ex(&ce, event_base)
                                : zend_register_internal_class(&ce);
    zend_declare_property_string(web_message_ce, "text", sizeof("text") - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_long(web_message_ce, "client", sizeof("client") - 1, 0, ZEND_ACC_PUBLIC);
    zend_declare_property_bool(web_message_ce, "binary", sizeof("binary") - 1, 0, ZEND_ACC_PUBLIC);
    return SUCCESS;
}

static const zend_function_entry web_functions[] = {
    PHP_FE(serve_http,   arginfo_serve_http)
    PHP_FE(serve_ws,     arginfo_serve_ws)
    PHP_FE(ws_broadcast, arginfo_ws_broadcast)
    PHP_FE_END
};

zend_module_entry web_module_entry = {
    STANDARD_MODULE_HEADER,
    "web",
    web_functions,
    PHP_MINIT(web),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};

#else
typedef int php_web_unused_t;   /* non-empty TU when not built */
#endif /* PHP_WEB_BUILD */
