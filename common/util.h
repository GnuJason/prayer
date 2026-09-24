/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#ifndef SUITE_UTIL_H
#define SUITE_UTIL_H

#include <stddef.h>
#include <time.h>

typedef struct {
    int year;
    int month;
    int day;
} Date;

int fail(const char *program, int status, const char *message);
int output_status(const char *program);
int parse_date(const char *text, Date *date);
int gregorian_valid(Date date);
int calendar_convert(Date input, int from_hijri, Date *output);
void date_string(Date date, char output[11]);
Date local_date(time_t instant);
int parse_number(const char *text, double minimum, double maximum, double *value);
int parse_instant(const char *text, time_t *instant);
int plain_text(const char *text, size_t capacity);
int select_timezone(const char *name);
void system_timezone(char *name, size_t capacity);
void json_string(const char *text);

#endif