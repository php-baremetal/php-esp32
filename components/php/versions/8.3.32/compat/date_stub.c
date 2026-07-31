/*
 * ext/date is off by default (build it in with -DPHP_EXT_DATE=ON), but a few core
 * files still call into it. When it's off, cover just those symbols:
 *
 *  - php_time()        -> time(NULL)
 *  - php_format_date() -> a minimal strftime-based formatter (UTC). It does NOT
 *                         interpret PHP's format characters; it always emits
 *                         "dd-Mon-YYYY HH:MM:SS UTC". Fine for log/cookie stamps.
 *  - timezone info     -> no database loaded, so these behave as "absent/UTC".
 *
 * If real date support is ever needed, expose a native function that reads the
 * RTC rather than pulling timelib and the tz data back in.
 */
#include "php.h"
#include "ext/date/php_date.h"

#include <time.h>

PHPAPI time_t php_time(void)
{
    return time(NULL);
}

PHPAPI zend_string *php_format_date(const char *format, size_t format_len, time_t ts, bool localtime)
{
    (void)format;
    (void)format_len;
    (void)localtime;

    struct tm tmv;
    char buf[64];

    gmtime_r(&ts, &tmv);
    size_t n = strftime(buf, sizeof(buf), "%d-%b-%Y %H:%M:%S UTC", &tmv);
    return zend_string_init(buf, n, 0);
}

PHPAPI timelib_tzinfo *get_timezone_info(void)
{
    return NULL;
}

timelib_time_offset *timelib_get_time_zone_info(timelib_sll ts, timelib_tzinfo *tz)
{
    (void)ts;
    (void)tz;
    return NULL;
}

void timelib_time_offset_dtor(timelib_time_offset *t)
{
    (void)t;
}
