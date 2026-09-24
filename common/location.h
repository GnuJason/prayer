/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#ifndef SUITE_LOCATION_H
#define SUITE_LOCATION_H

typedef struct {
    char city[256];
    char timezone[128];
    double latitude;
    double longitude;
} Location;

int location_config(const char *path, const char *city, Location *location,
                    char error[256]);
int location_auto(Location *location, char error[256]);

#endif