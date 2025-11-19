/*
 *  File: logue_fs.h
 *
 *  logue SDK 2.x file system utils 
 *
 *  2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_FILES 256

static inline __attribute__((optimize("Ofast"), always_inline))
bool is_ext(const char *name, const char *ext) {
    if (!name || !ext)
        return false;
    size_t namelen = strlen(name);
    size_t extlen = strlen(ext);
    if (namelen > extlen)
        return false;
    return strcmp(name + namelen - extlen, ext) == 0;
}

static inline __attribute__((optimize("Ofast"), always_inline))
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static inline __attribute__((optimize("Ofast"), always_inline))
char **getfilenames(const char *path, const char *ext) {
    static int count = 0;
    static char *names[MAX_FILES];
    DIR *dir;
    struct dirent *entry;

    while (count > 0)
        free(names[--count]);

    if ((dir = opendir(path)) == NULL)
        return NULL;

    while ((entry = readdir(dir)) != NULL && count < MAX_FILES) {
        if (entry->d_type != DT_REG || (ext != NULL && !is_ext(entry->d_name, ext)))
            continue;
        if ((names[count] = (char *)malloc(MAXNAMLEN + 1)) == NULL)
            break;
        strncpy(names[count++], entry->d_name, MAXNAMLEN + 1);
    }

    closedir(dir);
    qsort(names, count, sizeof(char *), compare_strings);
    return names;
}
