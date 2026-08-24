/*
 * store: a reboot-persistent key-value store for PHP, backed by ESP-IDF's NVS.
 *
 * Values written with store_set() survive a reset and come back with store_get() on the next boot.
 * The data lives in a dedicated `phpstore` NVS partition, whose size is set per project with
 * [store] size_kb (flash-tool passes -DPHP_STORE_KB, and cmake/gen-partitions.cmake adds the
 * partition). With no such partition -- persistence not configured -- the functions are inert and
 * store_available() returns false.
 *
 *   store_set(string $key, string $value): bool          persist a value (auto-committed)
 *   store_get(string $key, ?string $default = null)      read it back, or the default if absent
 *   store_has(string $key): bool
 *   store_delete(string $key): bool
 *   store_clear(): bool                                  wipe every key
 *   store_keys(): array                                  the keys currently stored
 *   store_available(): bool                              is persistence configured and ready?
 *
 * Keys are at most 15 characters (an NVS limit). Values are strings (NVS caps a string near 4 KB);
 * store numbers as strings and structures with json_encode(). It is wear-levelled storage for
 * configuration and state, not a log for high-frequency writes.
 */
#include "php.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define STORE_PARTITION "phpstore"
#define STORE_NAMESPACE "php"
#define STORE_KEY_MAX   15   /* NVS_KEY_NAME_MAX_SIZE is 16 including the terminator */

static const char *TAG = "store";
static nvs_handle_t s_handle;
static bool s_ok;

/* Empty arg list for the no-argument functions (PHP 8 warns without an arginfo). */
ZEND_BEGIN_ARG_INFO_EX(arginfo_store_none, 0, 0, 0)
ZEND_END_ARG_INFO()

static void store_init(void)
{
    esp_err_t err = nvs_flash_init_partition(STORE_PARTITION);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* A brand-new or outdated partition: format it and try once more. */
        nvs_flash_erase_partition(STORE_PARTITION);
        err = nvs_flash_init_partition(STORE_PARTITION);
    }
    if (err != ESP_OK) {
        /* Usually just "no such partition" -- persistence isn't configured. Stay quiet and inert. */
        s_ok = false;
        return;
    }
    if (nvs_open_from_partition(STORE_PARTITION, STORE_NAMESPACE, NVS_READWRITE, &s_handle) != ESP_OK) {
        s_ok = false;
        return;
    }
    s_ok = true;
    ESP_LOGI(TAG, "persistent store ready (partition '%s')", STORE_PARTITION);
}

static inline bool key_ok(size_t len)
{
    return s_ok && len > 0 && len <= STORE_KEY_MAX;
}

/* store_set(string $key, string $value): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_store_set, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
PHP_FUNCTION(store_set)
{
    char *key, *val;
    size_t klen, vlen;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(key, klen)
        Z_PARAM_STRING(val, vlen)
    ZEND_PARSE_PARAMETERS_END();

    if (!key_ok(klen)) {
        RETURN_FALSE;
    }
    esp_err_t err = nvs_set_str(s_handle, key, val);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    RETURN_BOOL(err == ESP_OK);
}

/* store_get(string $key, ?string $default = null): ?string */
ZEND_BEGIN_ARG_INFO_EX(arginfo_store_get, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, default)
ZEND_END_ARG_INFO()
PHP_FUNCTION(store_get)
{
    char *key;
    size_t klen;
    zval *def = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(key, klen)
        Z_PARAM_OPTIONAL
        Z_PARAM_ZVAL(def)
    ZEND_PARSE_PARAMETERS_END();

    if (key_ok(klen)) {
        size_t len = 0;
        if (nvs_get_str(s_handle, key, NULL, &len) == ESP_OK && len > 0) {
            char *buf = emalloc(len);
            if (nvs_get_str(s_handle, key, buf, &len) == ESP_OK) {
                RETVAL_STRINGL(buf, len - 1);   /* len includes the terminator */
                efree(buf);
                return;
            }
            efree(buf);
        }
    }
    if (def) {
        RETURN_ZVAL(def, 1, 0);
    }
    RETURN_NULL();
}

/* store_has(string $key): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_store_key, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()
PHP_FUNCTION(store_has)
{
    char *key;
    size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(key, klen)
    ZEND_PARSE_PARAMETERS_END();

    if (!key_ok(klen)) {
        RETURN_FALSE;
    }
    size_t len = 0;
    RETURN_BOOL(nvs_get_str(s_handle, key, NULL, &len) == ESP_OK);
}

/* store_delete(string $key): bool */
PHP_FUNCTION(store_delete)
{
    char *key;
    size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(key, klen)
    ZEND_PARSE_PARAMETERS_END();

    if (!key_ok(klen)) {
        RETURN_FALSE;
    }
    esp_err_t err = nvs_erase_key(s_handle, key);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    RETURN_BOOL(err == ESP_OK);
}

/* store_clear(): bool */
PHP_FUNCTION(store_clear)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!s_ok) {
        RETURN_FALSE;
    }
    esp_err_t err = nvs_erase_all(s_handle);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    RETURN_BOOL(err == ESP_OK);
}

/* store_keys(): array */
PHP_FUNCTION(store_keys)
{
    ZEND_PARSE_PARAMETERS_NONE();
    array_init(return_value);
    if (!s_ok) {
        return;
    }
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find(STORE_PARTITION, STORE_NAMESPACE, NVS_TYPE_ANY, &it);
    while (err == ESP_OK && it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        add_next_index_string(return_value, info.key);
        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
}

/* store_available(): bool */
PHP_FUNCTION(store_available)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(s_ok);
}

PHP_MINIT_FUNCTION(store)
{
    store_init();
    return SUCCESS;
}

static const zend_function_entry store_functions[] = {
    PHP_FE(store_set,       arginfo_store_set)
    PHP_FE(store_get,       arginfo_store_get)
    PHP_FE(store_has,       arginfo_store_key)
    PHP_FE(store_delete,    arginfo_store_key)
    PHP_FE(store_clear,     arginfo_store_none)
    PHP_FE(store_keys,      arginfo_store_none)
    PHP_FE(store_available, arginfo_store_none)
    PHP_FE_END
};

zend_module_entry store_module_entry = {
    STANDARD_MODULE_HEADER,
    "store",
    store_functions,
    PHP_MINIT(store),
    NULL, NULL, NULL, NULL,
    "0.1",
    STANDARD_MODULE_PROPERTIES,
};
