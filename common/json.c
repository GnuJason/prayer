/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#include "util.h"
#include <stdio.h>

void json_string(const char *text)
{
    if (!text) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor; cursor++) {
        if (*cursor == '"' || *cursor == '\\')
            printf("\\%c", *cursor);
        else if (*cursor < 32)
            printf("\\u%04x", *cursor);
        else
            putchar(*cursor);
    }
    putchar('"');
}