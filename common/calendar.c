/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#define HIJRI_IMPLEMENTATION
#include <hijri.h>
#include <prayertimes.h>
#include "util.h"
#include <math.h>

int calendar_convert(Date input, int from_hijri, Date *output)
{
    if (from_hijri) {
        HijriDate hijri = {input.year, input.month, input.day};
        if (input.year < 1 || input.year > 9999 || !hijri_tabular_date_valid(hijri))
            return 0;
        double julian = hijri_tabular_to_jd(hijri);
        mt_civil_from_days((long)floor(julian - 2440587.5),
                           &output->year, &output->month, &output->day);
        return gregorian_valid(*output);
    }
    if (!gregorian_valid(input))
        return 0;
    HijriDate hijri = hijri_tabular_from_jd(
        hijri_jd_from_gregorian(input.year, input.month, input.day));
    *output = (Date){hijri.year, hijri.month, hijri.day};
    return hijri.year >= 1 && hijri.year <= 9999;
}