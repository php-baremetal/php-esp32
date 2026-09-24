/*
 * The `http` source for the event-driven model: Baremetal\Http\Request / \Response and the global
 * serve_http() that starts an esp_http_server whose requests are delivered to the reactor as Request
 * events. The handler returns a Response, routed back to the same socket. Empty TU unless
 * PHP_WEB_BUILD is defined (the project enables [extensions.web]).
 */
#pragma once

#include "php.h"

extern zend_module_entry web_module_entry;
