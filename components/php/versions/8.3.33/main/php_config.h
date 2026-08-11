/*
 * Shim. Some headers (e.g. TSRM/TSRM.h) include "main/php_config.h" by an
 * explicit path; in a configure build that resolves to the generated config in
 * main/. We keep ours in the component root, so redirect there.
 */
#include "../php_config.h"
