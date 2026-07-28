/*
 * Minimal syslog(3): there's no syslog daemon here, so messages go to stderr
 * (which lands on the serial console). Enough to satisfy PHP's syslog()/openlog()
 * and let the engine link.
 */
#include "syslog.h"
#include <stdio.h>

static const char *s_ident = "php";
static int s_mask = 0xff;

void openlog(const char *ident, int option, int facility)
{
    (void)option;
    (void)facility;
    if (ident) {
        s_ident = ident;
    }
}

void vsyslog(int priority, const char *format, va_list ap)
{
    if (!(s_mask & LOG_MASK(LOG_PRI(priority)))) {
        return;
    }
    fprintf(stderr, "%s: ", s_ident);
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
}

void syslog(int priority, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsyslog(priority, format, ap);
    va_end(ap);
}

void closelog(void)
{
    s_ident = "php";
}

int setlogmask(int mask)
{
    int old = s_mask;
    if (mask != 0) {
        s_mask = mask;
    }
    return old;
}
