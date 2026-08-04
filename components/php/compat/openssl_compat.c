/*
 * openssl (compatible subset) -- a small, mbedTLS-backed stand-in for ext/openssl.
 *
 * This is NOT real OpenSSL: it provides just the symmetric-cipher functions that PHP web
 * frameworks lean on (e.g. Laravel's Encrypter) -- openssl_encrypt/openssl_decrypt for
 * AES-{128,192,256}-{CBC,GCM}, plus openssl_cipher_iv_length, openssl_random_pseudo_bytes and
 * openssl_error_string. No RSA, no X.509, no TLS. For the full API, build the real ext/openssl
 * against a ported OpenSSL library instead (-DPHP_EXT_OPENSSL_FULL); see docs/openssl.md.
 *
 * Built only when -DPHP_EXT_OPENSSL=ON and -DPHP_EXT_OPENSSL_FULL is off.
 */
#include "php.h"
#include "ext/standard/base64.h"   /* php_base64_encode / php_base64_decode */

#include "mbedtls/cipher.h"
#include "mbedtls/gcm.h"
#include "esp_random.h"            /* esp_fill_random -- the hardware RNG */

#include <string.h>

/* Flags, matching ext/openssl. */
#define OC_RAW_DATA      1
#define OC_ZERO_PADDING  2

/* Parse "aes-<bits>-<mode>" (case-insensitive). Fills is_gcm, key_bits and the default IV
 * length. Returns 1 on a supported cipher, 0 otherwise. */
static int oc_parse_cipher(const char *name, size_t len, int *is_gcm, int *key_bits, size_t *iv_len)
{
    char b[16];
    if (len == 0 || len >= sizeof b) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        b[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    b[len] = '\0';

    int bits;
    if      (!strncmp(b, "aes-128-", 8)) bits = 128;
    else if (!strncmp(b, "aes-192-", 8)) bits = 192;
    else if (!strncmp(b, "aes-256-", 8)) bits = 256;
    else return 0;

    const char *mode = b + 8;
    if      (!strcmp(mode, "cbc")) { *is_gcm = 0; *iv_len = 16; }
    else if (!strcmp(mode, "gcm")) { *is_gcm = 1; *iv_len = 12; }
    else return 0;

    *key_bits = bits;
    return 1;
}

/* Normalise a key to the cipher's exact length: zero-pad if short, truncate if long
 * (what OpenSSL does). keybuf must hold 32 bytes. */
static void oc_fix_key(unsigned char keybuf[32], const char *key, size_t key_len, int key_bits)
{
    size_t kl = (size_t) key_bits / 8;
    memset(keybuf, 0, 32);
    memcpy(keybuf, key, key_len < kl ? key_len : kl);
}

static mbedtls_cipher_type_t oc_cbc_type(int key_bits)
{
    return key_bits == 128 ? MBEDTLS_CIPHER_AES_128_CBC
         : key_bits == 192 ? MBEDTLS_CIPHER_AES_192_CBC
         :                    MBEDTLS_CIPHER_AES_256_CBC;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_openssl_cipher_iv_length, 0, 0, 1)
    ZEND_ARG_INFO(0, cipher_algo)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_openssl_encrypt, 0, 0, 3)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, cipher_algo)
    ZEND_ARG_INFO(0, passphrase)
    ZEND_ARG_INFO(0, options)
    ZEND_ARG_INFO(0, iv)
    ZEND_ARG_INFO(1, tag)          /* by-ref: GCM tag out */
    ZEND_ARG_INFO(0, aad)
    ZEND_ARG_INFO(0, tag_length)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_openssl_decrypt, 0, 0, 3)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, cipher_algo)
    ZEND_ARG_INFO(0, passphrase)
    ZEND_ARG_INFO(0, options)
    ZEND_ARG_INFO(0, iv)
    ZEND_ARG_INFO(0, tag)
    ZEND_ARG_INFO(0, aad)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_openssl_random_pseudo_bytes, 0, 0, 1)
    ZEND_ARG_INFO(0, length)
    ZEND_ARG_INFO(1, strong_result)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_openssl_error_string, 0, 0, 0)
ZEND_END_ARG_INFO()

/* openssl_cipher_iv_length(string $cipher_algo): int|false */
PHP_FUNCTION(openssl_cipher_iv_length)
{
    char *cipher;
    size_t cipher_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &cipher, &cipher_len) == FAILURE) {
        RETURN_FALSE;
    }
    int is_gcm, key_bits;
    size_t iv_len;
    if (!oc_parse_cipher(cipher, cipher_len, &is_gcm, &key_bits, &iv_len)) {
        RETURN_FALSE;
    }
    RETURN_LONG((zend_long) iv_len);
}

/* openssl_encrypt(string $data, string $cipher_algo, string $passphrase, int $options = 0,
 *                 string $iv = "", &$tag = null, string $aad = "", int $tag_length = 16) */
PHP_FUNCTION(openssl_encrypt)
{
    char *data, *cipher, *key, *iv = NULL, *aad = NULL;
    size_t data_len, cipher_len, key_len, iv_len = 0, aad_len = 0;
    zend_long options = 0, tag_len = 16;
    zval *tag_zv = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss|lszsl",
            &data, &data_len, &cipher, &cipher_len, &key, &key_len,
            &options, &iv, &iv_len, &tag_zv, &aad, &aad_len, &tag_len) == FAILURE) {
        RETURN_FALSE;
    }

    int is_gcm, key_bits;
    size_t def_iv;
    if (!oc_parse_cipher(cipher, cipher_len, &is_gcm, &key_bits, &def_iv)) {
        php_error_docref(NULL, E_WARNING, "Unknown cipher algorithm");
        RETURN_FALSE;
    }
    unsigned char keybuf[32];
    oc_fix_key(keybuf, key, key_len, key_bits);

    size_t outcap = data_len + 32;
    unsigned char *out = emalloc(outcap);
    size_t outlen = 0;
    unsigned char tagbuf[16];
    int rc;

    if (is_gcm) {
        if (tag_len < 4 || tag_len > 16) {
            tag_len = 16;
        }
        if (iv_len == 0) {
            efree(out);
            RETURN_FALSE;
        }
        mbedtls_gcm_context g;
        mbedtls_gcm_init(&g);
        rc = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, keybuf, (unsigned) key_bits);
        if (rc == 0) {
            rc = mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, data_len,
                    (const unsigned char *) iv, iv_len,
                    (const unsigned char *) aad, aad_len,
                    (const unsigned char *) data, out, (size_t) tag_len, tagbuf);
        }
        mbedtls_gcm_free(&g);
        outlen = data_len;
    } else {
        unsigned char ivbuf[16];
        memset(ivbuf, 0, sizeof ivbuf);
        if (iv) {
            memcpy(ivbuf, iv, iv_len < 16 ? iv_len : 16);
        }
        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        rc = mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(oc_cbc_type(key_bits)));
        if (rc == 0) rc = mbedtls_cipher_setkey(&ctx, keybuf, key_bits, MBEDTLS_ENCRYPT);
        if (rc == 0) rc = mbedtls_cipher_set_padding_mode(&ctx,
                (options & OC_ZERO_PADDING) ? MBEDTLS_PADDING_NONE : MBEDTLS_PADDING_PKCS7);
        if (rc == 0) rc = mbedtls_cipher_crypt(&ctx, ivbuf, 16,
                (const unsigned char *) data, data_len, out, &outlen);
        mbedtls_cipher_free(&ctx);
    }

    if (rc != 0) {
        efree(out);
        RETURN_FALSE;
    }

    if (is_gcm && tag_zv) {
        ZEND_TRY_ASSIGN_REF_STRINGL(tag_zv, (char *) tagbuf, (size_t) tag_len);
    }

    if (options & OC_RAW_DATA) {
        RETVAL_STRINGL((char *) out, outlen);
        efree(out);
    } else {
        zend_string *b64 = php_base64_encode(out, outlen);
        efree(out);
        RETURN_STR(b64);
    }
}

/* openssl_decrypt(string $data, string $cipher_algo, string $passphrase, int $options = 0,
 *                 string $iv = "", string $tag = "", string $aad = "") */
PHP_FUNCTION(openssl_decrypt)
{
    char *data, *cipher, *key, *iv = NULL, *tag = NULL, *aad = NULL;
    size_t data_len, cipher_len, key_len, iv_len = 0, tag_len = 0, aad_len = 0;
    zend_long options = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "sss|lsss",
            &data, &data_len, &cipher, &cipher_len, &key, &key_len,
            &options, &iv, &iv_len, &tag, &tag_len, &aad, &aad_len) == FAILURE) {
        RETURN_FALSE;
    }

    int is_gcm, key_bits;
    size_t def_iv;
    if (!oc_parse_cipher(cipher, cipher_len, &is_gcm, &key_bits, &def_iv)) {
        php_error_docref(NULL, E_WARNING, "Unknown cipher algorithm");
        RETURN_FALSE;
    }
    unsigned char keybuf[32];
    oc_fix_key(keybuf, key, key_len, key_bits);

    /* Input is base64 unless OPENSSL_RAW_DATA. */
    zend_string *decoded = NULL;
    const unsigned char *cbytes = (const unsigned char *) data;
    size_t clen = data_len;
    if (!(options & OC_RAW_DATA)) {
        decoded = php_base64_decode((const unsigned char *) data, data_len);
        if (!decoded) {
            RETURN_FALSE;
        }
        cbytes = (const unsigned char *) ZSTR_VAL(decoded);
        clen = ZSTR_LEN(decoded);
    }

    unsigned char *out = emalloc(clen + 16);
    size_t outlen = 0;
    int rc;

    if (is_gcm) {
        mbedtls_gcm_context g;
        mbedtls_gcm_init(&g);
        rc = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, keybuf, (unsigned) key_bits);
        if (rc == 0) {
            rc = mbedtls_gcm_auth_decrypt(&g, clen,
                    (const unsigned char *) iv, iv_len,
                    (const unsigned char *) aad, aad_len,
                    (const unsigned char *) tag, tag_len,
                    cbytes, out);
        }
        mbedtls_gcm_free(&g);
        outlen = clen;
    } else {
        unsigned char ivbuf[16];
        memset(ivbuf, 0, sizeof ivbuf);
        if (iv) {
            memcpy(ivbuf, iv, iv_len < 16 ? iv_len : 16);
        }
        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        rc = mbedtls_cipher_setup(&ctx, mbedtls_cipher_info_from_type(oc_cbc_type(key_bits)));
        if (rc == 0) rc = mbedtls_cipher_setkey(&ctx, keybuf, key_bits, MBEDTLS_DECRYPT);
        if (rc == 0) rc = mbedtls_cipher_set_padding_mode(&ctx,
                (options & OC_ZERO_PADDING) ? MBEDTLS_PADDING_NONE : MBEDTLS_PADDING_PKCS7);
        if (rc == 0) rc = mbedtls_cipher_crypt(&ctx, ivbuf, 16, cbytes, clen, out, &outlen);
        mbedtls_cipher_free(&ctx);
    }

    if (decoded) {
        zend_string_release(decoded);
    }
    if (rc != 0) {
        efree(out);
        RETURN_FALSE;   /* wrong key/tag/padding -> false, like OpenSSL */
    }
    RETVAL_STRINGL((char *) out, outlen);
    efree(out);
}

/* openssl_random_pseudo_bytes(int $length, &$strong_result = null): string|false */
PHP_FUNCTION(openssl_random_pseudo_bytes)
{
    zend_long length;
    zval *strong = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|z", &length, &strong) == FAILURE) {
        RETURN_FALSE;
    }
    if (length <= 0) {
        RETURN_FALSE;
    }
    zend_string *s = zend_string_alloc((size_t) length, 0);
    esp_fill_random(ZSTR_VAL(s), (size_t) length);   /* hardware RNG */
    ZSTR_VAL(s)[length] = '\0';
    if (strong) {
        ZEND_TRY_ASSIGN_REF_BOOL(strong, 1);
    }
    RETURN_STR(s);
}

/* openssl_error_string(): string|false -- no error queue here, always false. */
PHP_FUNCTION(openssl_error_string)
{
    RETURN_FALSE;
}

static const zend_function_entry openssl_functions[] = {
    PHP_FE(openssl_cipher_iv_length,     arginfo_openssl_cipher_iv_length)
    PHP_FE(openssl_encrypt,              arginfo_openssl_encrypt)
    PHP_FE(openssl_decrypt,              arginfo_openssl_decrypt)
    PHP_FE(openssl_random_pseudo_bytes,  arginfo_openssl_random_pseudo_bytes)
    PHP_FE(openssl_error_string,         arginfo_openssl_error_string)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(openssl)
{
    REGISTER_LONG_CONSTANT("OPENSSL_RAW_DATA",     OC_RAW_DATA,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("OPENSSL_ZERO_PADDING", OC_ZERO_PADDING, CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}

zend_module_entry openssl_module_entry = {
    STANDARD_MODULE_HEADER,
    "openssl",
    openssl_functions,
    PHP_MINIT(openssl),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "0.1-compat",
    STANDARD_MODULE_PROPERTIES
};
