/*
 *  File: vault.cc
 *
 *  Lirbarian unit for drumlogue and microKORG2
 * 
 *  2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include <string.h>
#include <stdio.h>

#include "logue_wrap.h"
#include "logue_perf.h"
#include "logue_fs.h"
#include "vault.h"

enum {
  param_export_bank = 0U,
#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
  param_export_data,
#endif
  param_export_file,
  param_export_run,
  param_import_bank,
#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
  param_import_data,
#endif
  param_import_file,
  param_import_run,
  param_set_number,
  param_package_type,
};  

enum {
  pending_none = 0U,
  pending_refresh,
  pending_export,
  pending_import,
};

#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
  static const char *resourceTypes[] = {"Programs", "Kits", "Both"};
#else
  static const char *bankNames[] {"Classic", "Modern", "Future", "User"};
#endif 

char package_prefix[4] = "";
const char *package_suffix = packageExtension[package_type_lib];
char package_name[MAXNAMLEN + 1];
static fs_dir package_list = fs_dir(packagePath, package_prefix, package_suffix);
static int32_t Params[PARAM_COUNT];
static uint32_t pending;
static bool suspended;
static int size;
static int sizes[RESOURCE_TYPE_COUNT];
static int offset;

__unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
  if (!desc)
    return k_unit_err_undef;
  if (desc->target != UNIT_HEADER_TARGET_FIELD)
    return k_unit_err_target;
  if (!UNIT_API_IS_COMPAT(desc->api))
    return k_unit_err_api_version;
  if (desc->samplerate != 48000)
    return k_unit_err_samplerate;
  if (desc->input_channels != UNIT_INPUT_CHANNELS || desc->output_channels != UNIT_OUTPUT_CHANNELS)
    return k_unit_err_geometry;

  return k_unit_err_none;
}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
  (void)in;
  (void)out;
  (void)frames;
  PERFMON_START
#ifdef PERFMON_ENABLE
  size_t total;
#endif

//Should normally never happen
  if (suspended)
    return;

  switch (pending) {
    case pending_export:
#ifdef PERFMON_ENABLE
      total = 
#endif
      vault_export(package_name, Params[param_package_type], sizes, offset);
    case pending_refresh:
      package_list.refresh(package_prefix, package_suffix);
      break;
    case pending_import:
#ifdef PERFMON_ENABLE
      total = 
#endif
      vault_import(package_name, Params[param_package_type], sizes, offset);
      break;
  }
  pending = pending_none;
  PERFMON_END(total)
}

static char *gen_package_name(char *buf, size_t bufsize, char *prefix, int size, int offset, int resource_type) {
  (void)resource_type;
  size_t pos = 0;
  pos += snprintf(buf + pos, bufsize - pos, "%s", prefix);
  switch(size) {
    case BANK_SIZE * BANK_COUNT:
      pos += snprintf(buf + pos, bufsize - pos, "All");
      break;
    case BANK_SIZE:
#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
      pos += snprintf(buf + pos, bufsize - pos, "Bank_%c", 'A' + ((offset / BANK_SIZE) & (BANK_COUNT - 1)));
      break;
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
      pos += snprintf(buf + pos, bufsize - pos, "%s", bankNames[(offset / BANK_SIZE)]);
      break;
    case GENRE_SIZE:
      pos += snprintf(buf + pos, bufsize - pos, "%s", bankNames[(offset / BANK_SIZE)]);
      pos += snprintf(buf + pos, bufsize - pos, "_%c", 'A' + ((offset / GENRE_SIZE) & (GENRE_COUNT - 1)) );
      break;
#endif
  }
#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  if (resource_type != RESOURCE_TYPE_COUNT)
    pos += snprintf(buf + pos, bufsize - pos, "_%s", resourceTypes[resource_type]);
#endif
  return buf;
}

__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
  PERFMON_RESET(PARAM_COUNT - 1, index, value)
  int resource_type;
  Params[index] = value;
  switch (index) {
    case param_export_run:
    case param_import_run:
      if (pending != pending_none || value != unit_header.params[index].max)
        break;

      value = Params[index - param_export_run + param_export_bank];
      if (value == 0)
        break;
      if (value == 1) {
        size = BANK_SIZE * BANK_COUNT;
        offset = 0;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
      } else if (value > BANK_COUNT) {
        value -= BANK_COUNT + 1;
        size = GENRE_SIZE;
        offset = value * GENRE_SIZE;
#endif
      } else {
        value--;
        size = BANK_SIZE;
        offset = value * BANK_SIZE; 
      }

      value = Params[index - param_export_run + param_export_file];
      if ((index == param_export_run && value > package_list.count) || (index == param_import_run && value >= package_list.count))
        break;

#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  resource_type = Params[index - param_export_run + param_export_data];
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
  resource_type = resource_type_program;
#endif

      if (index == param_export_run && value == 0) {
        gen_package_name(package_name, sizeof(package_name), package_prefix, size, offset, resource_type);
      } else {
        if (index == param_export_run)
          value--;
        strncpy(package_name, package_list.names[value], strrchr(package_list.names[value], '.') - package_list.names[value]);
      }

      for (int resourceType = 0; resourceType < RESOURCE_TYPE_COUNT; resourceType++)
        sizes[resourceType] = (resource_type == resourceType || resource_type == RESOURCE_TYPE_COUNT) ? size : 0;

      pending = index == param_export_run ? pending_export : pending_import;
      break;
    case param_set_number:
      if (pending == pending_none)
        break;
      if (value == 0)
        package_prefix[0] = 0;
      else
        snprintf(package_prefix, sizeof(package_prefix), "%02d_", value);
      pending = pending_refresh;
      break;
    case param_package_type:
      if (pending != pending_none)
        break;
      package_suffix = packageExtension[value];
      pending = pending_refresh;
      break;
  }
}

__unit_callback const char * unit_get_param_str_value(uint8_t index, int32_t value) {
  PERFMON_VALUE(PARAM_COUNT - 1, index, value)
  static const char *packageType[] = {"Library", "Preset"};
  static const char *runState[] = {"Run ->", "Exporting...", "Importing..."};
  static char s[16];
  switch (index) {
    case param_export_bank:
    case param_import_bank:
      if (value == 0) {
        return "None";
      } else if (value == 1) {
        return "All";
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
      } else if (value > BANK_COUNT + 1) {
        value -= BANK_COUNT + 2;
        snprintf(s, sizeof(s), "%s %c", bankNames[value / BANK_SIZE], 'A' + (value & (GENRE_COUNT - 1)));
      } else { 
        value -= 2;
        return bankNames[value];
#else 
      } else { 
        value -= 2;
        snprintf(s, sizeof(s), "Bank %c", 'A' + value);
#endif
      }
      return s;
    case param_export_file:
      if (value == 0)
        return ("Create New");
      value--;
    case param_import_file:
      if (value < package_list.count)
        return package_list.names[value];
      break;
    case param_import_run:
    case param_export_run:
      return runState[pending];
      break;
    case param_set_number:
      if (value == 0)
        return "All sets";
      snprintf(s, sizeof(s), "%d", value);
      return s;
    case param_package_type:
      return packageType[value];
      break;
#ifdef UNIT_TARGET_PLATFORM_DRUMLOGUE
    case param_export_data:
    case param_import_data:
      return resourceTypes[value];
#endif
  }
  return nullptr;
}

__unit_callback int32_t unit_get_param_value(uint8_t index) {
  return Params[index];
}

__unit_callback void unit_teardown() {
  pending = pending_none;
}

__unit_callback void unit_reset() {
  pending = pending_refresh;
}

__unit_callback void unit_resume() {
  suspended = false;
}

__unit_callback void unit_suspend() {
  suspended = true;
}
