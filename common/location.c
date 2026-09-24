/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Copyright (c) 2026 Jason <GnuJason>
 *
 * This file is part of the Islamic Utilities Suite.
 * It is licensed under the GNU General Public License v3.0 or later.
 */

#include "location.h"
#include "util.h"
#include <cJSON.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define JSON_LIMIT 65536

static int problem(char error[256], const char *message)
{
    snprintf(error, 256, "%s", message);
    return 0;
}

static int valid_utf8(const unsigned char *text, size_t length)
{
    for (size_t index = 0; index < length;) {
        unsigned value = text[index++];
        if (value < 128)
            continue;
        unsigned count, minimum;
        if (value >= 0xc2 && value <= 0xdf) {
            count = 1; minimum = 0x80; value &= 0x1f;
        } else if (value >= 0xe0 && value <= 0xef) {
            count = 2; minimum = 0x800; value &= 0x0f;
        } else if (value >= 0xf0 && value <= 0xf4) {
            count = 3; minimum = 0x10000; value &= 0x07;
        } else {
            return 0;
        }
        if (index + count > length)
            return 0;
        while (count--) {
            unsigned next = text[index++];
            if ((next & 0xc0) != 0x80)
                return 0;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return 0;
    }
    return 1;
}

static int unique_keys(const cJSON *object)
{
    if (!cJSON_IsObject(object))
        return 0;
    for (const cJSON *entry = object->child; entry; entry = entry->next) {
        for (const cJSON *other = entry->next; other; other = other->next) {
            if (!strcmp(entry->string, other->string))
                return 0;
        }
    }
    return 1;
}

static cJSON *parse_json(char *buffer, size_t length)
{
    buffer[length] = '\0';
    if (memchr(buffer, '\0', length) || strstr(buffer, "\\u0000")
        || !valid_utf8((const unsigned char *)buffer, length))
        return NULL;
    return cJSON_ParseWithLengthOpts(buffer, length + 1, NULL, 1);
}

static int extract_location(const cJSON *object, const char *city,
                            int geome, Location *location, char error[256])
{
    if (!unique_keys(object))
        return problem(error, "location must be a JSON object with unique keys");
    const cJSON *latitude = cJSON_GetObjectItemCaseSensitive(object, geome ? "lat" : "latitude");
    const cJSON *longitude = cJSON_GetObjectItemCaseSensitive(object, geome ? "lon" : "longitude");
    const cJSON *timezone = cJSON_GetObjectItemCaseSensitive(object, "timezone");
    if (!cJSON_IsNumber(latitude) || !isfinite(latitude->valuedouble)
        || fabs(latitude->valuedouble) > 90
        || !cJSON_IsNumber(longitude) || !isfinite(longitude->valuedouble)
        || fabs(longitude->valuedouble) > 180)
        return problem(error, "location requires finite latitude [-90,90] and longitude [-180,180]");
    if (!plain_text(city, sizeof(location->city)))
        return problem(error, "location requires a nonempty city without control characters");
    if (timezone && (!cJSON_IsString(timezone)
        || !plain_text(timezone->valuestring, sizeof(location->timezone))))
        return problem(error, "timezone must be a nonempty IANA zone name");
    if (!geome && !timezone)
        return problem(error, "configured locations require a timezone");
    location->latitude = latitude->valuedouble;
    location->longitude = longitude->valuedouble;
    snprintf(location->city, sizeof(location->city), "%s", city);
    if (timezone)
        snprintf(location->timezone, sizeof(location->timezone), "%s", timezone->valuestring);
    return 1;
}

int location_config(const char *path, const char *city, Location *location,
                    char error[256])
{
    char default_path[4096];
    char buffer[JSON_LIMIT + 2];
    if (!path) {
        const char *base = getenv("XDG_CONFIG_HOME");
        int length;
        if (base && base[0] == '/')
            length = snprintf(default_path, sizeof(default_path), "%s/prayer/locations.json", base);
        else {
            base = getenv("HOME");
            if (!base || base[0] != '/')
                return problem(error, "set HOME or use --config for offline locations");
            length = snprintf(default_path, sizeof(default_path), "%s/.config/prayer/locations.json", base);
        }
        if (length < 0 || (size_t)length >= sizeof(default_path))
            return problem(error, "configuration path is too long");
        path = default_path;
    }
    FILE *file = fopen(path, "rb");
    if (!file)
        return problem(error, "cannot open locations config; use --auto, --coordinates, or --config");
    size_t length = fread(buffer, 1, JSON_LIMIT + 1, file);
    int read_error = ferror(file);
    fclose(file);
    if (read_error || length > JSON_LIMIT)
        return problem(error, "cannot read configuration or it exceeds 64 KiB");
    cJSON *root = parse_json(buffer, length);
    int success = 0;
    if (!unique_keys(root)) {
        problem(error, "invalid configuration JSON (object with unique keys required)");
        goto done;
    }
    if (!city) {
        const cJSON *default_city = cJSON_GetObjectItemCaseSensitive(root, "default");
        if (!cJSON_IsString(default_city)) {
            problem(error, "missing location; specify --location, --auto, or a config default");
            goto done;
        }
        city = default_city->valuestring;
    }
    const cJSON *locations = cJSON_GetObjectItemCaseSensitive(root, "locations");
    if (!unique_keys(locations)) {
        problem(error, "configuration requires a locations object with unique keys");
        goto done;
    }
    const cJSON *entry = cJSON_GetObjectItemCaseSensitive(locations, city);
    if (!entry) {
        problem(error, "city not configured; geome 1.0 has no city lookup; add coordinates to the config");
        goto done;
    }
    success = extract_location(entry, city, 0, location, error);
done:
    cJSON_Delete(root);
    return success;
}

static long long monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return -1;
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int run_geome(char buffer[JSON_LIMIT + 2], size_t *length, char error[256])
{
    int descriptors[2];
    if (pipe(descriptors))
        return problem(error, "cannot create geome pipe");
    pid_t child = fork();
    if (child == -1) {
        close(descriptors[0]); close(descriptors[1]);
        return problem(error, "cannot start geome");
    }
    if (child == 0) {
        (void)setpgid(0, 0);
        close(descriptors[0]);
        if (descriptors[1] != STDOUT_FILENO) {
            if (dup2(descriptors[1], STDOUT_FILENO) == -1)
                _exit(126);
            close(descriptors[1]);
        }
        execlp("geome", "geome", "--json", (char *)NULL);
        _exit(127);
    }
    (void)setpgid(child, child);
    close(descriptors[1]);
    long long start = monotonic_ms();
    int eof = 0, status = 0;
    *length = 0;
    for (;;) {
        long long now = monotonic_ms();
        if (start < 0 || now < 0 || now - start >= 15000) {
            problem(error, "geome timed out (15 seconds)");
            break;
        }
        struct pollfd descriptor = {.fd = descriptors[0], .events = POLLIN};
        int ready = poll(eof ? NULL : &descriptor, eof ? 0 : 1, 20);
        if (ready < 0 && errno != EINTR) {
            problem(error, "cannot read geome output");
            break;
        }
        if (ready > 0) {
            ssize_t count = read(descriptors[0], buffer + *length, JSON_LIMIT + 1 - *length);
            if (count < 0 && errno != EINTR) {
                problem(error, "cannot read geome output");
                break;
            }
            if (count == 0)
                eof = 1;
            if (count > 0)
                *length += (size_t)count;
            if (*length > JSON_LIMIT) {
                problem(error, "geome output exceeds 64 KiB");
                break;
            }
        }
        if (eof) {
            pid_t result = waitpid(child, &status, WNOHANG);
            if (result == child) {
                close(descriptors[0]);
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                    return 1;
                return problem(error, WIFEXITED(status) && WEXITSTATUS(status) == 127
                    ? "geome not installed or not on PATH; use an offline location"
                    : "geome failed; check its diagnostics or use an offline location");
            }
            if (result < 0 && errno != EINTR) {
                problem(error, "cannot collect geome exit status");
                break;
            }
        }
    }
    (void)kill(-child, SIGKILL);
    (void)kill(child, SIGKILL);
    close(descriptors[0]);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return 0;
}

int location_auto(Location *location, char error[256])
{
    char buffer[JSON_LIMIT + 2];
    size_t length;
    if (!run_geome(buffer, &length, error))
        return 0;
    cJSON *root = parse_json(buffer, length);
    const cJSON *city = cJSON_GetObjectItemCaseSensitive(root, "city");
    int success = extract_location(root, cJSON_IsString(city) ? city->valuestring : NULL,
                                   1, location, error);
    cJSON_Delete(root);
    return success;
}