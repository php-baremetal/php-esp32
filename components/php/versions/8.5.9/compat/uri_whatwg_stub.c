/*
 * php-esp32: stub for PHP 8.5's WHATWG URL parser (ext/uri/uri_parser_whatwg.c).
 *
 * The real parser is backed by lexbor, whose ~370 KB of static tables overflow the ESP32's internal
 * RAM. This firmware therefore drops lexbor and the WHATWG parser: the RFC 3986 parser (uriparser)
 * and the legacy parse_url() parser -- the one the core stream layer actually uses -- remain fully
 * functional. Uri\WhatWg\Url still registers as a class, but constructing one raises a clear error.
 *
 * php_uri.c references exactly three symbols from the WHATWG parser, none of them lexbor-typed (the
 * php_uri_parser interface uses void* handles): the parser struct, its RINIT, and its post-deactivate
 * hook. This file provides those. It deliberately does NOT include uri_parser_whatwg.h, which would
 * pull in lexbor/url/url.h.
 */
#include "php.h"
#include "php_uri_common.h"

static void *whatwg_parse(const char *uri_str, size_t uri_str_len, const void *base_url, zval *errors, bool silent)
{
	(void) uri_str; (void) uri_str_len; (void) base_url; (void) errors;
	if (!silent) {
		zend_throw_error(NULL,
			"The WHATWG URL parser is not available in this firmware (lexbor is not built); "
			"use Uri\\Rfc3986\\Uri or parse_url() instead");
	}
	return NULL;
}
static void *whatwg_clone(void *uri) { (void) uri; return NULL; }
static zend_string *whatwg_to_string(void *uri, php_uri_recomposition_mode mode, bool exclude_fragment)
{
	(void) uri; (void) mode; (void) exclude_fragment; return NULL;
}
static void whatwg_destroy(void *uri) { (void) uri; }
static zend_result whatwg_read(void *uri, php_uri_component_read_mode read_mode, zval *retval)
{
	(void) uri; (void) read_mode; (void) retval; return FAILURE;
}
static zend_result whatwg_write(void *uri, zval *value, zval *errors)
{
	(void) uri; (void) value; (void) errors; return FAILURE;
}

PHPAPI const php_uri_parser php_uri_parser_whatwg = {
	.name = PHP_URI_PARSER_WHATWG,
	.parse = whatwg_parse,
	.clone = whatwg_clone,
	.to_string = whatwg_to_string,
	.destroy = whatwg_destroy,
	{
		.scheme   = { .read = whatwg_read, .write = whatwg_write },
		.username = { .read = whatwg_read, .write = whatwg_write },
		.password = { .read = whatwg_read, .write = whatwg_write },
		.host     = { .read = whatwg_read, .write = whatwg_write },
		.port     = { .read = whatwg_read, .write = whatwg_write },
		.path     = { .read = whatwg_read, .write = whatwg_write },
		.query    = { .read = whatwg_read, .write = whatwg_write },
		.fragment = { .read = whatwg_read, .write = whatwg_write },
	}
};

PHP_RINIT_FUNCTION(uri_parser_whatwg) { return SUCCESS; }
ZEND_MODULE_POST_ZEND_DEACTIVATE_D(uri_parser_whatwg) { return SUCCESS; }
