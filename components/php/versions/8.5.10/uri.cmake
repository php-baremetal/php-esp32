# PHP 8.5: ext/uri is a built-in the core now depends on (streams, the ftp/http stream wrappers,
# ext/filter's URL validation). Upstream ships two parser backends: uriparser (RFC 3986) and lexbor's
# WHATWG URL parser.
#
# This firmware builds uriparser (RFC 3986) plus the legacy parse_url() parser -- the one the core
# stream layer actually uses -- and DROPS lexbor: its ~370 KB of static tables overflow the ESP32's
# internal RAM. Uri\WhatWg\Url still registers as a class, but a stub replaces the parser so
# constructing one raises a clear error (see compat/uri_whatwg_stub.c). parse_url(),
# FILTER_VALIDATE_URL, the stream wrappers, and Uri\Rfc3986\Uri all work.
#
# Paths are absolute (${PHP_COMPONENT_DIR}/${PHP_SRC}): set_source_files_properties INCLUDE_DIRECTORIES
# rejects relative paths, and ${PHP_SRC} is relative to the component here.

set(_uri_root "${PHP_COMPONENT_DIR}/${PHP_SRC}")

file(GLOB _uriparser_srcs "${_uri_root}/ext/uri/uriparser/src/*.c")

set(_uri_srcs
    ${_uriparser_srcs}
    "${_uri_root}/ext/uri/php_uri.c"
    "${_uri_root}/ext/uri/php_uri_common.c"
    "${_uri_root}/ext/uri/uri_parser_rfc3986.c"
    "${_uri_root}/ext/uri/uri_parser_php_parse_url.c"
    "${PHP_COMPONENT_DIR}/${PHP_VER_DIR}/compat/uri_whatwg_stub.c")   # replaces the lexbor-backed parser

target_sources(${COMPONENT_LIB} PRIVATE ${_uri_srcs})
# ext/lexbor is on the include path only so php_uri.c's #include of uri_parser_whatwg.h (which pulls
# lexbor/url/url.h) still finds the header; no lexbor source is compiled or linked.
set_source_files_properties(${_uri_srcs} PROPERTIES
    INCLUDE_DIRECTORIES "${_uri_root}/ext/uri;${_uri_root}/ext/uri/uriparser/include;${_uri_root}/ext/lexbor"
    COMPILE_DEFINITIONS "URI_STATIC_BUILD;URI_ENABLE_ANSI;URI_NO_UNICODE")
