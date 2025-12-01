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
#include <thread>

struct fs_dir {
  int count;
  struct dirent **dirlist;
  const char *path;
  struct {
    const char *prefix;
    const char *suffix;
  } filter;

  static const fs_dir *&self() {
      static thread_local const fs_dir *ptr = nullptr;
      return ptr;
  }

  static int flt(const struct dirent *entry) {
    const fs_dir *s = self();
    return entry->d_type == DT_REG
      && (s->filter.prefix == nullptr || strncmp(entry->d_name, s->filter.prefix, strlen(s->filter.prefix)) == 0)
      && (s->filter.suffix == nullptr || strcmp(entry->d_name + strlen(entry->d_name) - strlen(s->filter.suffix), s->filter.suffix) == 0);
  }

  void cleanup() {
    if (dirlist == nullptr)
      return;
    free(dirlist);
    dirlist = nullptr;
  }

  char *get(int index) {
    return dirlist[index]->d_name;
  }

  void remove(int index) {
    for (; index < count - 1; index++)
      dirlist[index] = dirlist[index + 1];
    count--;
  }

  void refresh() {
    cleanup();
    self() = this;
    count = scandir(path, &dirlist, flt, alphasort);
    self() = nullptr;
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
    dirlist = nullptr;
    refresh();
  }

  fs_dir(const char *pth, const char *pfx, const char *sfx) : path(pth), filter({.prefix = pfx, .suffix = sfx}) {
    init();
  }

  fs_dir(const char *pth, const char *sfx) : path(pth), filter({.prefix = nullptr, .suffix = sfx}) {
    init();
  }

  fs_dir(const char *pth) : path(pth) {
    init();
  }

  fs_dir() : dirlist(nullptr), filter({.prefix = nullptr, .suffix = nullptr}) {}

  ~fs_dir() {
    cleanup();
  }

};
