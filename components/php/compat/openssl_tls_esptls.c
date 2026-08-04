/*
 * Real TLS client stream transport for ext/openssl, backed by ESP-IDF's esp-tls / mbedTLS.
 *
 * openssl.c's MINIT registers the ssl://, tls://, tlsv1.x://, ... stream transports pointing at
 * php_openssl_ssl_socket_factory. The crypto-only build stubs that factory (see openssl_ssl_stub.c);
 * THIS file provides a working one when built with PHP_EXT_OPENSSL_TLS, so PHP can actually reach
 * HTTPS: file_get_contents("https://..."), stream_socket_client("tls://host:443"), etc.
 *
 * Why esp-tls instead of OpenSSL's own libssl+xp_ssl.c: libssl needs BSD sockets, which live in
 * ESP-IDF's lwIP -- not in the bare riscv32-esp-elf sysroot the standalone OpenSSL cross-build sees
 * (that's why fetch-openssl.sh configures `no-sock`). esp-tls is built inside ESP-IDF where lwIP and
 * mbedTLS are available, and does DNS + TCP connect + TLS handshake in one call. So the crypto API
 * stays real OpenSSL (libcrypto), and the transport rides mbedTLS -- both proven on this target.
 *
 * Certificate verification: the CA bundle path comes from the PHP_TLS_CAFILE env var (main.c points
 * it at the certs the project ships, see docs/openssl.md). With no bundle, esp-tls connects without
 * verifying the peer (logged) -- functional but insecure, so ship the certs for anything real.
 */
#include "php.h"
#include "php_streams.h"
#include "php_network.h"
#include "php_openssl.h"

#include "esp_tls.h"
#include "esp_log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TLS_TAG = "php-tls";

/* The CA bundle (PEM), loaded once from PHP_TLS_CAFILE and cached for every connection. ca_bytes
 * counts the trailing NUL, as esp-tls wants for a PEM buffer. Both stay zero if there's no file. */
static unsigned char *g_ca_buf = NULL;
static unsigned int    g_ca_bytes = 0;
static int             g_ca_loaded = 0;

static void tls_load_ca_once(void)
{
	if (g_ca_loaded) {
		return;
	}
	g_ca_loaded = 1;
	const char *path = getenv("PHP_TLS_CAFILE");
	if (!path || !*path) {
		ESP_LOGW(TLS_TAG, "no PHP_TLS_CAFILE -- TLS peers will NOT be verified");
		return;
	}
	FILE *f = fopen(path, "rb");
	if (!f) {
		ESP_LOGW(TLS_TAG, "CA bundle %s not found -- TLS peers will NOT be verified", path);
		return;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0) {
		fclose(f);
		ESP_LOGW(TLS_TAG, "CA bundle %s is empty", path);
		return;
	}
	unsigned char *buf = malloc((size_t) sz + 1);
	if (!buf) {
		fclose(f);
		return;
	}
	size_t rd = fread(buf, 1, (size_t) sz, f);
	fclose(f);
	buf[rd] = '\0';
	g_ca_buf = buf;
	g_ca_bytes = (unsigned int) rd + 1;   /* PEM: include the NUL terminator */
	ESP_LOGI(TLS_TAG, "loaded CA bundle %s (%u bytes)", path, (unsigned) rd);
}

/* Per-connection state hung off the php_stream. */
typedef struct {
	esp_tls_t *tls;
	char      *host;   /* SNI / verification hostname, for logging */
	int        eof;
} php_esptls_data;

/* Parse "host:port" (proto already stripped). Fills host (caller frees) and port; -1 on error. */
static int tls_split_hostport(const char *name, size_t namelen, char **host_out, int *port_out)
{
	if (!name || namelen == 0) {
		return -1;
	}
	/* be defensive if a "scheme://" slipped through */
	const char *p = name;
	const char *sep = strstr(name, "://");
	if (sep && (size_t)(sep - name) < namelen) {
		p = sep + 3;
	}
	const char *colon = strrchr(p, ':');
	if (!colon || colon == p) {
		return -1;
	}
	int port = atoi(colon + 1);
	if (port <= 0 || port > 65535) {
		return -1;
	}
	size_t hlen = (size_t)(colon - p);
	char *host = malloc(hlen + 1);
	if (!host) {
		return -1;
	}
	memcpy(host, p, hlen);
	host[hlen] = '\0';
	*host_out = host;
	*port_out = port;
	return 0;
}

static ssize_t php_esptls_write(php_stream *stream, const char *buf, size_t count)
{
	php_esptls_data *d = stream->abstract;
	if (!d || !d->tls) {
		return -1;
	}
	size_t sent = 0;
	while (sent < count) {
		ssize_t r = esp_tls_conn_write(d->tls, buf + sent, count - sent);
		if (r > 0) {
			sent += (size_t) r;
			continue;
		}
		if (r == ESP_TLS_ERR_SSL_WANT_WRITE || r == ESP_TLS_ERR_SSL_WANT_READ) {
			continue;   /* blocking transport: retry */
		}
		break;          /* real error */
	}
	return sent > 0 ? (ssize_t) sent : -1;
}

static ssize_t php_esptls_read(php_stream *stream, char *buf, size_t count)
{
	php_esptls_data *d = stream->abstract;
	if (!d || !d->tls) {
		return -1;
	}
	ssize_t r = esp_tls_conn_read(d->tls, buf, count);
	if (r > 0) {
		return r;
	}
	if (r == ESP_TLS_ERR_SSL_WANT_READ || r == ESP_TLS_ERR_SSL_WANT_WRITE) {
		return 0;       /* no data right now, not EOF */
	}
	/* 0 == clean close, negative == error: either way the stream is done. */
	d->eof = 1;
	stream->eof = 1;
	return r < 0 ? -1 : 0;
}

static int php_esptls_close(php_stream *stream, int close_handle)
{
	php_esptls_data *d = stream->abstract;
	if (!d) {
		return 0;
	}
	if (close_handle && d->tls) {
		esp_tls_conn_destroy(d->tls);
	}
	free(d->host);
	efree(d);
	stream->abstract = NULL;
	return 0;
}

static int php_esptls_flush(php_stream *stream)
{
	(void) stream;
	return 0;
}

static int php_esptls_cast(php_stream *stream, int castas, void **ret)
{
	php_esptls_data *d = stream->abstract;
	int fd = -1;
	if (d && d->tls) {
		esp_tls_get_conn_sockfd(d->tls, &fd);
	}
	switch (castas) {
		case PHP_STREAM_AS_FD:
		case PHP_STREAM_AS_FD_FOR_SELECT:
		case PHP_STREAM_AS_SOCKETD:
			if (fd < 0) {
				return FAILURE;
			}
			if (ret) {
				*(php_socket_t *)ret = fd;
			}
			return SUCCESS;
		default:
			return FAILURE;
	}
}

/* Connect + TLS handshake for the ssl:// family, driven from the XPORT_API set_option op. */
static int php_esptls_connect(php_stream *stream, php_stream_xport_param *xparam)
{
	php_esptls_data *d = stream->abstract;
	char *host = NULL;
	int port = 0;

	if (tls_split_hostport(xparam->inputs.name, xparam->inputs.namelen, &host, &port) != 0) {
		ESP_LOGE(TLS_TAG, "bad connect target");
		return -1;
	}

	tls_load_ca_once();

	esp_tls_cfg_t cfg = {0};
	if (g_ca_buf) {
		cfg.cacert_buf   = g_ca_buf;
		cfg.cacert_bytes = g_ca_bytes;
	} else {
		cfg.skip_common_name = true;   /* no bundle -> don't verify (insecure, already warned) */
	}
	if (xparam->inputs.timeout) {
		cfg.timeout_ms = (int) (xparam->inputs.timeout->tv_sec * 1000 +
		                        xparam->inputs.timeout->tv_usec / 1000);
	}

	esp_tls_t *tls = esp_tls_init();
	if (!tls) {
		free(host);
		return -1;
	}
	int ok = esp_tls_conn_new_sync(host, (int) strlen(host), port, &cfg, tls);
	if (ok != 1) {
		ESP_LOGE(TLS_TAG, "handshake to %s:%d failed", host, port);
		esp_tls_conn_destroy(tls);
		free(host);
		return -1;
	}
	d->tls  = tls;
	d->host = host;
	ESP_LOGI(TLS_TAG, "TLS connected to %s:%d", host, port);
	return 0;
}

static int php_esptls_set_option(php_stream *stream, int option, int value, void *ptrparam)
{
	php_esptls_data *d = stream->abstract;
	php_stream_xport_param *xparam;

	switch (option) {
		case PHP_STREAM_OPTION_XPORT_API:
			xparam = (php_stream_xport_param *) ptrparam;
			switch (xparam->op) {
				case STREAM_XPORT_OP_CONNECT:
				case STREAM_XPORT_OP_CONNECT_ASYNC:
					xparam->outputs.returncode = php_esptls_connect(stream, xparam);
					return PHP_STREAM_OPTION_RETURN_OK;
				default:
					return PHP_STREAM_OPTION_RETURN_NOTIMPL;
			}

		case PHP_STREAM_OPTION_CRYPTO_API:
			/* ssl:// implies TLS -- the handshake already ran in connect. Report success so the
			 * http/ssl wrappers that ask to "enable crypto" are satisfied. */
			return PHP_STREAM_OPTION_RETURN_OK;

		case PHP_STREAM_OPTION_CHECK_LIVENESS:
			return (d && d->tls && !d->eof) ? PHP_STREAM_OPTION_RETURN_OK
			                                : PHP_STREAM_OPTION_RETURN_ERR;

		case PHP_STREAM_OPTION_BLOCKING:
			/* esp-tls runs blocking; accept the request without changing anything. */
			return PHP_STREAM_OPTION_RETURN_OK;

		case PHP_STREAM_OPTION_READ_TIMEOUT:
			return PHP_STREAM_OPTION_RETURN_OK;

		case PHP_STREAM_OPTION_META_DATA_API:
			add_assoc_bool((zval *) ptrparam, "timed_out", 0);
			add_assoc_bool((zval *) ptrparam, "blocked", 1);
			add_assoc_bool((zval *) ptrparam, "eof", d ? d->eof : 1);
			return PHP_STREAM_OPTION_RETURN_OK;

		default:
			return PHP_STREAM_OPTION_RETURN_NOTIMPL;
	}
}

static const php_stream_ops php_esptls_ops = {
	php_esptls_write, php_esptls_read,
	php_esptls_close, php_esptls_flush,
	"tcp_socket/ssl",          /* label userland sees via stream_get_meta_data */
	NULL,                      /* seek */
	php_esptls_cast,
	NULL,                      /* stat */
	php_esptls_set_option,
};

php_stream *php_openssl_ssl_socket_factory(const char *proto, size_t protolen,
		const char *resourcename, size_t resourcenamelen,
		const char *persistent_id, int options, int flags,
		struct timeval *timeout,
		php_stream_context *context STREAMS_DC)
{
	(void) proto; (void) protolen; (void) resourcename; (void) resourcenamelen;
	(void) options; (void) flags; (void) timeout; (void) context;

	php_esptls_data *d = ecalloc(1, sizeof(*d));
	php_stream *stream = php_stream_alloc_rel(&php_esptls_ops, d, persistent_id, "r+");
	if (!stream) {
		efree(d);
		return NULL;
	}
	return stream;
}
