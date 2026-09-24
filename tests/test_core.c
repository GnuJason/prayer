/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    Date input, output, reverse;
    char text[11];
    assert(parse_date("2024-03-11", &input));
    assert(calendar_convert(input, 0, &output));
    assert(output.year == 1445 && output.month == 9 && output.day == 1);
    assert(calendar_convert(output, 1, &reverse));
    assert(reverse.year == 2024 && reverse.month == 3 && reverse.day == 11);
    date_string(output, text);
    assert(strcmp(text, "1445-09-01") == 0);
    assert(!parse_date("2024-3-11", &input));
    assert(!parse_date("2024-03-11x", &input));
    assert(!gregorian_valid((Date){1900, 2, 29}));
    assert(gregorian_valid((Date){2000, 2, 29}));
    assert(!calendar_convert((Date){1445, 2, 30}, 1, &output));
    assert(calendar_convert((Date){1, 1, 1}, 1, &output));
    assert(output.year == 622 && output.month == 7 && output.day == 19);
    assert(calendar_convert(output, 0, &reverse));
    assert(reverse.year == 1 && reverse.month == 1 && reverse.day == 1);
    assert(select_timezone("America/New_York"));
    assert(!select_timezone("Invalid/Zone"));
    assert(!select_timezone("../etc/passwd"));
    puts("core tests passed");
    return 0;
}