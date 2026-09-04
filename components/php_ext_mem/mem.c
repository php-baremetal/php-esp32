/*
 * mem: a volatile in-RAM key-value store for PHP, shared across the requests of a single boot.
 *
 * It is the RAM twin of the NVS-backed `store_*`. In the web-server model each HTTP request is a
 * fresh PHP request cycle, so userland variables do not survive between requests; mem_* gives a
 * place to hand data from one request (or the server-init script) to the next without touching
 * flash. The table lives in persistent (non-request) memory -- so it survives php_request_shutdown()
 * -- and is wiped on reboot. Nothing here wears the flash, so unlike store_* it is fine to write on
 * every request (a hit counter, a small cache, a rate-limit bucket).
 *
 *   mem_set(string $key, mixed $value): bool     store a value (serialized; a copy, not a live ref)
 *   mem_get(string $key, mixed $default = null)  read it back (a fresh copy), or the default
 *   mem_has(string $key): bool
 *   mem_delete(string $key): bool
 *   mem_clear(): bool                            drop every key
 *   mem_keys(): array                            the keys currently stored
 *
 * Values are stored with PHP's serializer, so scalars, arrays and serializable objects work; each
 * mem_get() returns an independent copy (there is no shared live object -- an object graph cannot
 * outlive a request). Live resources/handles (a socket, a display) are NOT serializable and must
 * live in a C extension instead. The table is not persistent across reboots and is not thread-safe,
 * but the web-server runs requests one at a time, so no locking is needed here.
 */
#include "php.h"
#include "ext/standard/php_var.h"   /* php_var_serialize / php_var_unserialize */
#include "zend_smart_str.h"

/* The store: string key -> a persistent zend_string holding the serialized value. Allocated with
 * the persistent flag so it (and its keys/buckets) outlive any single request. */
static HashTable s_mem;
static bool s_ready;

/* Free one stored blob when a key is overwritten, deleted, or the table is cleared. */
static void mem_val_dtor(zval *zv)
{
    zend_string *blob = Z_PTR_P(zv);
    if (blob) {
        zend_string_release_ex(blob, 1 /* persistent */);
    }
}

/* mem_clear/mem_keys/mem_available take no arguments; the return type differs, so one arginfo each. */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_clear, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_keys, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_available, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

/* mem_set(string $key, mixed $value): bool */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_set, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()
PHP_FUNCTION(mem_set)
{
    char *key;
    size_t klen;
    zval *val;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(key, klen)
        Z_PARAM_ZVAL(val)
    ZEND_PARSE_PARAMETERS_END();

    if (!s_ready || klen == 0) {
        RETURN_FALSE;
    }

    smart_str buf = {0};
    php_serialize_data_t var_hash;
    PHP_VAR_SERIALIZE_INIT(var_hash);
    php_var_serialize(&buf, val, &var_hash);
    PHP_VAR_SERIALIZE_DESTROY(var_hash);
    if (!buf.s) {
        smart_str_free(&buf);
        RETURN_FALSE;
    }

    /* A persistent copy of the serialized bytes, owned by the table (freed by mem_val_dtor). */
    zend_string *blob = zend_string_init(ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), 1 /* persistent */);
    smart_str_free(&buf);

    /* update_ptr overwrites (and destructs) any previous value under this key; the table copies
     * the key with its own (persistent) allocation. */
    zend_hash_str_update_ptr(&s_mem, key, klen, blob);
    RETURN_TRUE;
}

/* mem_get(string $key, mixed $default = null): mixed */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_get, 0, 1, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, default, IS_MIXED, 0, "null")
ZEND_END_ARG_INFO()
PHP_FUNCTION(mem_get)
{
    char *key;
    size_t klen;
    zval *def = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(key, klen)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(def)
    ZEND_PARSE_PARAMETERS_END();

    if (s_ready && klen > 0) {
        zend_string *blob = zend_hash_str_find_ptr(&s_mem, key, klen);
        if (blob) {
            const unsigned char *p = (const unsigned char *) ZSTR_VAL(blob);
            const unsigned char *end = p + ZSTR_LEN(blob);
            php_unserialize_data_t var_hash;
            PHP_VAR_UNSERIALIZE_INIT(var_hash);
            bool ok = php_var_unserialize(return_value, &p, end, &var_hash);
            PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
            if (ok) {
                return;   /* return_value now holds a fresh copy */
            }
            zval_ptr_dtor(return_value);   /* corrupt/partial -- fall through to the default */
            ZVAL_UNDEF(return_value);
        }
    }
    if (def) {
        RETURN_ZVAL(def, 1, 0);
    }
    RETURN_NULL();
}

/* mem_has(string $key): bool -- also used for mem_delete (same signature). */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_mem_key, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()
PHP_FUNCTION(mem_has)
{
    char *key;
    size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(key, klen)
    ZEND_PARSE_PARAMETERS_END();

    RETURN_BOOL(s_ready && klen > 0 && zend_hash_str_exists(&s_mem, key, klen));
}

/* mem_delete(string $key): bool */
PHP_FUNCTION(mem_delete)
{
    char *key;
    size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(key, klen)
    ZEND_PARSE_PARAMETERS_END();

    if (!s_ready || klen == 0) {
        RETURN_FALSE;
    }
    RETURN_BOOL(zend_hash_str_del(&s_mem, key, klen) == SUCCESS);
}

/* mem_clear(): bool */
PHP_FUNCTION(mem_clear)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_ready) {
        RETURN_FALSE;
    }
    zend_hash_clean(&s_mem);
    RETURN_TRUE;
}

/* mem_keys(): array */
PHP_FUNCTION(mem_keys)
{
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
    if (!s_ready) {
        return;
    }
    zend_string *key;
    ZEND_HASH_FOREACH_STR_KEY(&s_mem, key) {
        if (key) {
            /* A fresh request-scoped copy of the key: the stored key lives in persistent memory, so
             * copy its bytes rather than sharing it into this request's array. */
            add_next_index_stringl(return_value, ZSTR_VAL(key), ZSTR_LEN(key));
        }
    } ZEND_HASH_FOREACH_END();
}

/* mem_available(): bool -- the in-RAM store is up (always true once the module started). */
PHP_FUNCTION(mem_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(s_ready);
}

PHP_MINIT_FUNCTION(mem)
{
    zend_hash_init(&s_mem, 8, NULL, mem_val_dtor, 1 /* persistent */);
    s_ready = true;
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(mem)
{
    if (s_ready) {
        zend_hash_destroy(&s_mem);
        s_ready = false;
    }
    return SUCCESS;
}

static const zend_function_entry mem_functions[] = {
    PHP_FE(mem_set,       arginfo_mem_set)
    PHP_FE(mem_get,       arginfo_mem_get)
    PHP_FE(mem_has,       arginfo_mem_key)
    PHP_FE(mem_delete,    arginfo_mem_key)
    PHP_FE(mem_clear,     arginfo_mem_clear)
    PHP_FE(mem_keys,      arginfo_mem_keys)
    PHP_FE(mem_available, arginfo_mem_available)
    PHP_FE_END
};

zend_module_entry mem_module_entry = {
    STANDARD_MODULE_HEADER,
    "mem",
    mem_functions,
    PHP_MINIT(mem),
    PHP_MSHUTDOWN(mem),
    NULL, NULL, NULL,
    "1.0",
    STANDARD_MODULE_PROPERTIES,
};
