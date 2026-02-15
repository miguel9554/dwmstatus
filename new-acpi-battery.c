/* new-acpi-battery.c
 *
 * Drop-in replacement for reading battery info from sysfs.
 * Provides readfile(), getbattery(), and a small smprintf() helper.
 *
 * Compile with the same flags you used before.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* smprintf: allocate a formatted string (caller must free) */
static char *
smprintf(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    char *buf;
    int len;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (!buf) {
        va_end(ap2);
        return NULL;
    }

    vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

/* helper to trim trailing whitespace/newline in-place */
static void
rtrim(char *s)
{
    int i = (int)strlen(s) - 1;
    while (i >= 0 && (s[i] == '\n' || s[i] == '\r' || s[i] == ' ' || s[i] == '\t'))
        s[i--] = '\0';
}

/*
 * readfile: read the first line of base/file, trim newline, return malloc'd string.
 * On failure returns NULL (caller should handle).
 */
static char *
readfile(char *base, char *file)
{
    char *path;
    char line[513];
    FILE *fd;

    path = smprintf("%s/%s", base, file);
    if (!path)
        return NULL;

    fd = fopen(path, "r");
    if (fd == NULL) {
        free(path);
        return NULL;
    }

    if (fgets(line, sizeof(line) - 1, fd) == NULL) {
        /* empty file or read error */
        fclose(fd);
        free(path);
        return NULL;
    }
    fclose(fd);
    free(path);

    rtrim(line);
    return smprintf("%s", line);
}

/*
 * getbattery: return a malloc'd string with battery percentage (rounded to 0 decimals),
 * optionally with remaining time in hours in parentheses (e.g. "63% (2.5h)").
 * On error, return "not present" or "invalid". Caller should free().
 */
char *
getbattery(char *base)
{
    char *co = NULL;
    long long full = -1, now = -1, power = -1;
    double pct;

    /* check presence file first */
    co = readfile(base, "present");
    if (co) {
        if (co[0] != '1') {
            free(co);
            return smprintf("not present");
        }
        free(co);
    }

    /* 1) try capacity (percentage already provided by some drivers) */
    co = readfile(base, "capacity");
    if (co) {
        char *endp;
        long val = strtol(co, &endp, 10);
        if (endp != co && *endp == '\0') {
            /* try to include time estimate if possible */
            long long energy_now = -1, power_now = -1;
            free(co);

            co = readfile(base, "energy_now");
            if (co) {
                energy_now = strtoll(co, NULL, 10);
                free(co);
            }

            co = readfile(base, "power_now");
            if (co) {
                power_now = strtoll(co, NULL, 10);
                free(co);
            }

            if (energy_now > 0 && power_now > 0) {
                double hours = (double)energy_now / (double)power_now;
                return smprintf("%ld%% (%.1fh)", val, hours);
            }

            return smprintf("%ld%%", val);
        }
        free(co);
    }

    /* Try energy_* first (modern) */
    co = readfile(base, "energy_full_design");
    if (!co) co = readfile(base, "energy_full");
    if (co) {
        full = strtoll(co, NULL, 10);
        free(co);
    }

    co = readfile(base, "energy_now");
    if (co) {
        now = strtoll(co, NULL, 10);
        free(co);
    }

    /* if energy didn't provide both values, try charge_* */
    if (full <= 0 || now < 0) {
        full = -1;
        now = -1;

        co = readfile(base, "charge_full_design");
        if (!co) co = readfile(base, "charge_full");
        if (co) {
            full = strtoll(co, NULL, 10);
            free(co);
        }

        co = readfile(base, "charge_now");
        if (co) {
            now = strtoll(co, NULL, 10);
            free(co);
        }
    }

    if (full <= 0 || now < 0) {
        return smprintf("invalid");
    }

    pct = ((double) now / (double) full) * 100.0;
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;

    /* read power_now to compute time estimate */
    co = readfile(base, "power_now");
    if (co) {
        power = strtoll(co, NULL, 10);
        free(co);
    }

    if (power > 0) {
        double hours = (double)now / (double)power; /* µWh / µW = hours */
        return smprintf("%.0f%% (%.1fh)", pct, hours);
    }

    return smprintf("%.0f%%", pct);
}

