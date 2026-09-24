/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#include "util.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int fail(const char *program, int status, const char *message)
{
    fprintf(stderr, "%s: %s\n", program, message);
    return status;
}

int output_status(const char *program)
{
    return fflush(stdout) == EOF || ferror(stdout)
        ? fail(program, 3, "cannot write output") : 0;
}

int parse_date(const char *text, Date *date)
{
    if (strlen(text) != 10 || text[4] != '-' || text[7] != '-')
        return 0;
    for (size_t index = 0; index < 10; index++) {
        if (index != 4 && index != 7 && !isdigit((unsigned char)text[index]))
            return 0;
    }
    return sscanf(text, "%4d-%2d-%2d", &date->year, &date->month, &date->day) == 3
        && date->year >= 1 && date->month >= 1 && date->month <= 12
        && date->day >= 1 && date->day <= 31;
}

int gregorian_valid(Date date)
{
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (date.year < 1 || date.year > 9999 || date.month < 1 || date.month > 12)
        return 0;
    int length = lengths[date.month - 1];
    if (date.month == 2 && date.year % 4 == 0
        && (date.year % 100 != 0 || date.year % 400 == 0))
        length++;
    return date.day >= 1 && date.day <= length;
}

void date_string(Date date, char output[11])
{
    snprintf(output, 11, "%04u-%02u-%02u", (unsigned)date.year % 10000,
             (unsigned)date.month % 100, (unsigned)date.day % 100);
}

Date local_date(time_t instant)
{
    struct tm local;
    if (!localtime_r(&instant, &local))
        return (Date){0, 0, 0};
    return (Date){local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

int parse_number(const char *text, double minimum, double maximum, double *value)
{
    char *end;
    errno = 0;
    double number = strtod(text, &end);
    if (!*text || isspace((unsigned char)*text) || *end || errno
        || !isfinite(number) || number < minimum || number > maximum)
        return 0;
    *value = number;
    return 1;
}

int parse_instant(const char *text, time_t *instant)
{
    char *end;
    errno = 0;
    long long number = strtoll(text, &end, 10);
    if (!*text || isspace((unsigned char)*text) || *end || errno
        || (long long)(time_t)number != number)
        return 0;
    *instant = (time_t)number;
    return 1;
}

int plain_text(const char *text, size_t capacity)
{
    if (!text || !*text || strlen(text) >= capacity)
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor; cursor++) {
        if (*cursor < 32 || *cursor == 127)
            return 0;
    }
    return 1;
}

int select_timezone(const char *name)
{
    char path[512];
    unsigned char magic[4];
    if (!plain_text(name, 128) || name[0] == '/' || strstr(name, ".."))
        return 0;
    for (const char *cursor = name; *cursor; cursor++) {
        if (!isalnum((unsigned char)*cursor) && !strchr("/_+-", *cursor))
            return 0;
    }
    snprintf(path, sizeof(path), "/usr/share/zoneinfo/%s", name);
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;
    int valid = fread(magic, 1, sizeof(magic), file) == sizeof(magic)
        && memcmp(magic, "TZif", sizeof(magic)) == 0;
    fclose(file);
    if (!valid || setenv("TZ", name, 1) != 0)
        return 0;
    tzset();
    return 1;
}

void system_timezone(char *name, size_t capacity)
{
    const char *environment = getenv("TZ");
    char path[512];
    if (environment && plain_text(environment, capacity)) {
        snprintf(name, capacity, "%s", environment);
        return;
    }
    if (environment && !*environment) {
        snprintf(name, capacity, "UTC");
        return;
    }
    ssize_t length = readlink("/etc/localtime", path, sizeof(path) - 1);
    if (length > 0) {
        path[length] = '\0';
        const char *zone = strstr(path, "zoneinfo/");
        if (zone && plain_text(zone + 9, capacity)) {
            snprintf(name, capacity, "%s", zone + 9);
            return;
        }
    }
    snprintf(name, capacity, "system");
}