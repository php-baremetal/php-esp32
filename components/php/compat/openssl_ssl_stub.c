/*
 * Stub for ext/openssl's TLS stream transport factory (normally in xp_ssl.c).
 *
 * The full openssl build here is crypto-only (PHP_EXT_OPENSSL_FULL): it compiles openssl.c and
 * links libcrypto, but NOT xp_ssl.c/libssl (which would pull the TLS state machine and BSD
 * sockets). openssl.c's MINIT still registers the ssl://, tls://, ... stream transports pointing
 * at this factory, so this provides it -- it just refuses to open. Every crypto function
 * (openssl_encrypt/decrypt, digest, sign/verify, pkey, x509, csr, ...) works; only wrapping a
 * socket in TLS via fopen("ssl://...") is unavailable. Build with libssl to get that too.
 */
#include "php.h"
#include "php_openssl.h"

#include <stdint.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include "esp_random.h"

/* libssl entry point that openssl.c's MINIT calls to init TLS strings/algorithms. We don't link
 * libssl (crypto-only build), so provide a no-op -- the crypto library inits itself on use. */
int OPENSSL_init_ssl(uint64_t opts, const void *settings)
{
	(void) opts; (void) settings;
	return 1;
}

/*
 * Entropy. OpenSSL was configured with --with-rand-seed=none (no /dev/urandom on this target),
 * so at runtime RAND_bytes() has no seed source and would fail. We install a RAND_METHOD backed
 * by the ESP32 hardware RNG (esp_fill_random) at load time, so key/IV/nonce generation works.
 */
static int oc_rand_bytes(unsigned char *buf, int num)
{
	if (num < 0) {
		return 0;
	}
	esp_fill_random(buf, (size_t) num);
	return 1;
}
static int oc_rand_status(void) { return 1; }

static RAND_METHOD oc_rand_method = {
	NULL,           /* seed   */
	oc_rand_bytes,  /* bytes  */
	NULL,           /* cleanup*/
	NULL,           /* add    */
	oc_rand_bytes,  /* pseudorand */
	oc_rand_status, /* status */
};

__attribute__((constructor))
static void oc_openssl_full_init(void)
{
#ifdef PHP_EXT_OPENSSL_NO_LOAD_CONFIG
	/* Optional (the openssl `no_load_config` setting): skip loading openssl.cnf entirely, for a
	 * firmware that ships no config file. The built-in default provider still loads. Off by
	 * default -- the default is to read an openssl.cnf (OPENSSL_CONF, set by main.c). */
	OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, NULL);
#endif
	/* Legacy RAND_bytes path -> hardware RNG. */
	RAND_set_rand_method(&oc_rand_method);
	/* Seed the OpenSSL 3.0 provider DRBG (used by key generation) with hardware entropy up front,
	 * on top of the getrandom() seed source, so openssl_pkey_new() has entropy immediately. */
	unsigned char seed[48];
	esp_fill_random(seed, sizeof seed);
	RAND_add(seed, (int) sizeof seed, (double) sizeof seed);
}

php_stream *php_openssl_ssl_socket_factory(const char *proto, size_t protolen,
		const char *resourcename, size_t resourcenamelen,
		const char *persistent_id, int options, int flags,
		struct timeval *timeout,
		php_stream_context *context STREAMS_DC)
{
	(void) proto; (void) protolen; (void) resourcename; (void) resourcenamelen;
	(void) persistent_id; (void) options; (void) flags; (void) timeout; (void) context;
	/* TLS stream transport not built (crypto-only openssl). */
	return NULL;
}
