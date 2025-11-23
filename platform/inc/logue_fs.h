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

struct fs_dir {
  static fs_dir *self;
  int count;
  char **names;
  const char *path;
  struct {
    const char *prefix;
    const char *suffix;
  } filter;

  static int flt(const struct dirent *entry) {
    return entry->d_type == DT_REG
      && (self->filter.prefix == NULL || strncmp(entry->d_name, self->filter.prefix, strlen(self->filter.prefix)) == 0)
      && (self->filter.suffix == NULL || strcmp(entry->d_name + strlen(entry->d_name) - strlen(self->filter.suffix), self->filter.suffix) == 0);
  }

  static int cmp(const struct dirent **a, const struct dirent **b) {
    return strcmp((*a)->d_name, (*b)->d_name);
  }

  void cleanup() {
    if (names)
      for (; count; free(names[--count]));
    free(names);
  }

  void remove(int index) {
    for (; index < count - 1; index++)
      names[index] = names[index + 1];
    count--;
  }

  void refresh() {
    struct dirent **dirlist;
    cleanup();
    count = scandir(path, &dirlist, flt, cmp);
    names = (char **)malloc(sizeof(char *) * count); 
    if (count && names)
      for (int i = 0; i < count; i++)
        names[i] = strdup(dirlist[i]->d_name);
    for (int i = 0; i < count; free(dirlist[i++]));
    free(dirlist);
  }

  void refresh(const char *suffix) {
    filter.suffix = suffix;
    refresh();
  }

  void refresh(const char *prefix, const char *suffix) {
    filter.prefix = prefix;
    filter.suffix = suffix;
    refresh();
  }

  void init() {
    self = this;
    names = NULL;
    refresh();
  }

  fs_dir(const char *pth, const char *pfx, const char *sfx) : path(pth), filter({.prefix = pfx, .suffix = sfx}) {
    init();
  }

  fs_dir(const char *pth, const char *sfx) : path(pth), filter({.prefix = NULL, .suffix = sfx}) {
    init();
  }

  fs_dir(const char *pth) : path(pth) {
    init();
  }

  ~fs_dir() {
    cleanup();
  }

} *fs_dir::self = nullptr;
