/*
 *  File: vault.h
 *
 *  Lirbarian unit for drumlogue and microKORG2
 * 
 *  2025 (c) Oleg Burdaev
 *  mailto: dukesrg@gmail.com
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "zip.c"
#pragma GCC diagnostic pop
#include "logue_fs.h"

#ifndef VAULT_COMPRESSION_LEVEL
#define VAULT_COMPRESSION_LEVEL ZIP_DEFAULT_COMPRESSION_LEVEL
#endif
#ifndef VAULT_CHUNK_SIZE
#define VAULT_CHUNK_SIZE 1024
#endif
#define VAULT_BUF_SIZE 65536

#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
#include "init_drmlg.h"
#define PRODUCT_NAME "drumlogue"
#define PRODUCT_CODE "drmlg"
#define RESOURCE_TYPE_COUNT 2
#ifndef PACKAGE_PATH
#define PACKAGE_PATH "/var/lib/drumlogued/userfs/Programs"
#endif
#ifndef RESOURCE_PATH
#define RESOURCE_PATH "/var/lib/drumlogued/userfs/Programs", "/var/lib/drumlogued/userfs/Kits"
#endif
#define RESOURCE_INFO_FILE_CONTENT "<" PRODUCT_NAME "_ProgramInformation/>", "<" PRODUCT_NAME "_KitInformation/>"
#define RESOURCE_INFO_FILE_NAME_TEMPLATE "Prog_%03d.prog_info", "Kit_%03d.kit_info"
#define RESOURCE_BIN_FILE_NAME_TEMPLATES "Prog_%03d.prog_bin", "Kit_%03d.kit_bin"
#define RESOURCE_FILE_PREFIX "%c%02d_"
#define RESOURCE_FILE_HEADER "DLOGPRG\x00\x00\x03\x01\x00", "DLOGKIT\x00\x00\x00\x01\x00"
#define RESOURCE_FILE_EXTENSION "." PRODUCT_CODE "prog", "." PRODUCT_CODE "kit"
#define RESOURCE_NAME_OFFSET 4, 4
#define RESOURCE_NAME_LENGTH 20, 20
#define RESOURCE_INIT_DATA Init_Program_drmlg, Init_Kit_drmlg
#define RESOURCE_INIT_DATA_SIZE Init_Program_drmlg_len, Init_Kit_drmlg_len
#define CONTENT_META_TEMPLATES " NumProgramData=\"%d\"", " NumKitData=\"%d\""
#define CONTENT_DATA_TEMPLATES \
  "    <ProgramData>\n" \
  "      <Information>%s</Information>\n" \
  "      <ProgramBinary>%s</ProgramBinary>\n" \
  "    </ProgramData>\n" \
  , \
  "    <KitData>\n" \
  "      <Information>%s</Information>\n" \
  "      <KitBinary>%s</KitBinary>\n" \
  "    </KitData>\n"
#define BANK_SIZE 16
#define GENRE_SIZE 0
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
#include "init_mk2.h"
#define PRODUCT_NAME "microKORG2"
#define PRODUCT_CODE "mk2"
#define RESOURCE_TYPE_COUNT 1
#ifndef PACKAGE_PATH
#define PACKAGE_PATH "/var/lib/microkorgd/userfs/Programs"
#endif
#ifndef RESOURCE_PATH
#define RESOURCE_PATH "/var/lib/microkorgd/userfs/Programs"
#endif
#define RESOURCE_INFO_FILE_CONTENT "<" PRODUCT_NAME "_ProgramInformation/>"
#define RESOURCE_INFO_FILE_NAME_TEMPLATE "Prog_%03d.prog_info"
#define RESOURCE_BIN_FILE_NAME_TEMPLATES "Prog_%03d.prog_bin"
#define RESOURCE_FILE_PREFIX "%c_%c%d_"
#define RESOURCE_FILE_HEADER "MK2PROG\x00\x00\x01\x00\x02"
#define RESOURCE_FILE_EXTENSION "." PRODUCT_CODE "prog"
#define RESOURCE_NAME_OFFSET 4
#define RESOURCE_NAME_LENGTH 20
#define RESOURCE_INIT_DATA Init_Program_mk2
#define RESOURCE_INIT_DATA_SIZE Init_Program_mk2_len
#define CONTENT_META_TEMPLATES " NumProgramData=\"%d\""
#define CONTENT_DATA_TEMPLATES \
  "    <ProgramData>\n" \
  "      <Information>%s</Information>\n" \
  "      <ProgramBinary>%s</ProgramBinary>\n" \
  "    </ProgramData>\n"
#define BANK_SIZE 64
#define GENRE_SIZE 8
static const char *bankLetters = "CMFU";
static const char *favoriteFileName = "FavoriteData.fav_data";
static const char *favoriteFileContent = "<minilogue_Favorite/>";
#endif

#define RESOURCE_HEADER_SIZE 16
#define RESOURCE_HEADER_LENGTH_SIZE 4
#define RESOURCE_HEADER_CRC_SIZE 4

#define PRESET_INFO_FILE_CONTENT \
  "<" PRODUCT_NAME "_Preset>\n" \
  "  <DataID>%s</DataID>\n" \
  "  <Name>%s</Name>\n" \
  "</" PRODUCT_NAME "_Preset>\n"

#include <time.h>
struct timespec tss, tse;

static const char *fileInfoFileName = "FileInformation.xml";
static const char *presetInfoFileName = "PresetInformation.xml";
static const char *resourcePath[] = {RESOURCE_PATH};
static const char *resourceInfoFileNameTemplate[] = {RESOURCE_INFO_FILE_NAME_TEMPLATE};
static const char *resourceBinFileNameTemplate[] = {RESOURCE_BIN_FILE_NAME_TEMPLATES};
static const char *contentMetaTemplates[] = {CONTENT_META_TEMPLATES};
static const char *contentDataTemplates[] = {CONTENT_DATA_TEMPLATES};
static const char *resourceInfoFileContent[] = {RESOURCE_INFO_FILE_CONTENT};
static const char *resourceFileHeader[12] = {RESOURCE_FILE_HEADER};
static const char *resourceFileExtension[] = {RESOURCE_FILE_EXTENSION};
static const int resourceNameOffset[] = {RESOURCE_NAME_OFFSET};
static const int resourceNameLength[] = {RESOURCE_NAME_LENGTH};
static const char *packagePath = PACKAGE_PATH;
static const char *packageExtension[] = {"." PRODUCT_CODE "lib", "." PRODUCT_CODE "preset",};
static const unsigned char *resourceInitData[] = {RESOURCE_INIT_DATA};
static const unsigned int resourceInitDataSize[] = {RESOURCE_INIT_DATA_SIZE};

enum {
  package_type_lib = 0U,
  package_type_preset,
  package_type_num
};

enum {
  resource_type_program = 0U,
  resource_type_kit
};

static char *getResourceFileName(char *data, int index, int resourceType) {
  static char filename[MAXNAMLEN + 1];
  int len = sizeof(filename), pos;
#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
  pos = snprintf(filename, len, RESOURCE_FILE_PREFIX, 'A' + (index >> 4), (index & 0x0F) + 1);
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
  pos = snprintf(filename, len, RESOURCE_FILE_PREFIX, bankLetters[index >> 6], 'A' + ((index >> 3) & 0x07), (index & 0x07) + 1);
#endif
  pos += snprintf(filename + pos, resourceNameLength[resourceType], "%s", data + resourceNameOffset[resourceType]);
  snprintf(filename + pos, len - pos, "%s", resourceFileExtension[resourceType]);

  for (char *c = filename; *c != 0; c++)
    if (*c == ' ')
      *c = '_';

  return filename;
}

char *getInfoFileName(char *filename, int index, int resourceType) {
  snprintf(filename, MAXNAMLEN + 1, resourceInfoFileNameTemplate[resourceType], index);
  return filename;
}

char *getBinFileName(char *filename, int index, int resourceType) {
  snprintf(filename, MAXNAMLEN + 1, resourceBinFileNameTemplate[resourceType], index);
  return filename;
}

size_t getFileInfoContent(int *contentCounts, int packageType, char *buf, size_t bufsize) {
  size_t pos = 0;
  char contentInfo[1024];
  char resourceInfoFileName[MAXNAMLEN + 1];
  char resourceBinFileName[MAXNAMLEN + 1];

  if (packageType == package_type_preset)
    pos += snprintf(contentInfo + pos, sizeof(contentInfo) - pos, " NumPresetInformation=\"1\"");

  for (int resourceType = 0; resourceType < RESOURCE_TYPE_COUNT; resourceType++)
    if (contentCounts[resourceType] > 0)
      pos += snprintf(contentInfo + pos, sizeof(contentInfo) - pos, contentMetaTemplates[resourceType], contentCounts[resourceType]);

  pos = snprintf(buf, bufsize - pos,
    "<KorgMSLibrarian_Data>\n"
    "  <Product>" PRODUCT_NAME "</Product>\n"
    "  <Contents%s%s>\n",
    contentInfo,
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
    packageType == package_type_lib ? " NumFavoriteData=\"1\"" :
#endif
    "");
#define PRESET_INFO_HEADER \
"    <PresetInformation>\n" \
"      <File>"
#define PRESET_INFO_FILENAME "PresetInformation.xml"
#define PRESET_INFO_FOOTER \
"</File>\n" \
"    </PresetInformation>\n"
      
    if (packageType == package_type_preset) {
    pos += snprintf(buf + pos, bufsize - pos,
      "    <PresetInformation>\n"
      "      <File>%s</File>\n"
      "    </PresetInformation>\n",
      presetInfoFileName);
  }

  for (int resourceType = 0; resourceType < RESOURCE_TYPE_COUNT; resourceType++)
    for (int index = 0; index < contentCounts[resourceType]; index++)
      pos += snprintf(buf + pos, bufsize - pos, contentDataTemplates[resourceType], getInfoFileName(resourceInfoFileName, index, resourceType), getBinFileName(resourceBinFileName, index, resourceType));

#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
  if (packageType == package_type_lib)
    pos += snprintf(buf + pos, bufsize - pos, \
      "    <FavoriteData>\n" \
      "      <File>%s</File>\n" \
      "    </FavoriteData>\n",
     favoriteFileName);
#endif

  pos += snprintf(buf + pos, bufsize - pos,
    "  </Contents>\n"
    "</KorgMSLibrarian_Data>\n"
  );

  return pos;
}

enum {
  vault_idle = 0L,
  vault_export_start,
  vault_export_package_open,
  vault_export_gen_fileinfo,
  vault_export_open_fileinfo,
  vault_export_write_fileinfo_next_chunk,
  vault_export_write_fileinfo_last_chunk,
  vault_export_write_fileinfo_close,
  vault_export_write_presetinfo,
  vault_export_dir_open,
  vault_export_write_infofile,
  vault_export_read_binfile,
  vault_export_open_binfile_entry,
  vault_export_write_binfile_next_chunk,
  vault_export_write_binfile_last_chunk,
  vault_export_close_binfile_entry,
  vault_export_entry_next,//
  vault_export_package_close,
  vault_export_finished,
  vault_import_start,
  vault_import_package_open,
  vault_import_dir_open,
  vault_import_entry_open, //22
  vault_import_entry_read_first_chunk,
  vault_import_entry_file_open,
  vault_import_entry_write_1st_chunk,
  vault_import_entry_process_next_chunk,
  vault_import_entry_process_last_chunk,
  vault_import_entry_close,
  vault_import_entry_remove_duplicate,
  vault_import_entry_next,//
  vault_import_package_close,
  vault_import_finished
};

struct vault {
  int state = vault_idle;
  const char *packageName;
  int packageType;
  int *resourceTypeCount;
  int offset;

  char *path;
  struct zip_t *zip;
  int resourceType;
  int headerCrc;
  fs_dir dir;
  int resourceIndex;
  char *buf;
  size_t bufsize;
  size_t size;
  size_t pos;
  size_t firstchunksize;
  int crc;
  int dir_index;
  char *resourceFileName;
  FILE *fp;
  struct stat st;
  const unsigned char *src;

  vault() : state(vault_idle), path(nullptr), buf(nullptr) {}

  ~vault() {
    if (path != nullptr)
      free(path);
    path = nullptr;
    if (buf != nullptr)
      free(buf);
    buf = nullptr;
  }

  int init(const char *packageName, int packageType, int *resourceTypeCount, int offset, int state) {
    if (this->state != vault_idle)
      return vault_idle;
    this->packageName = packageName;
    this->packageType = packageType;
    this->resourceTypeCount = resourceTypeCount;
    this->offset = offset;
    if (path != nullptr)
      free(path);
    path = nullptr;
    if (buf != nullptr)
      free(buf);
    buf = nullptr;
    return this->state = state;
  }

  int process() {
    switch (state) {
      case vault_export_start:
      case vault_import_start:
        bufsize = state == vault_import_start ? VAULT_CHUNK_SIZE : VAULT_BUF_SIZE;
        buf = (char*)malloc(bufsize);
        path = (char*)malloc(MAXNAMLEN + 1);
        snprintf(path, MAXNAMLEN + 1, "%s/%s%s", packagePath, packageName, packageExtension[packageType]);
        break;
      case vault_export_package_open:
#ifndef VAULT_WRITE_DISABLE
        zip = zip_open(path, VAULT_COMPRESSION_LEVEL, 'w');
#else
        zip = zip_stream_open(NULL, 0, VAULT_COMPRESSION_LEVEL, 'w');
#endif
        break;
      case vault_export_gen_fileinfo:
        size = getFileInfoContent(resourceTypeCount, packageType, buf, bufsize);
        break;
      case vault_export_open_fileinfo:
        zip_entry_open(zip, fileInfoFileName);
        pos = 0;
        break;
      case vault_export_write_fileinfo_next_chunk:
        if ((int)size < VAULT_CHUNK_SIZE)
          break;
        zip_entry_write(zip, buf + pos, VAULT_CHUNK_SIZE);
        size -= VAULT_CHUNK_SIZE;
        pos += VAULT_CHUNK_SIZE;
        return state;
      case vault_export_write_fileinfo_last_chunk:
        if ((int)size > 0)
          zip_entry_write(zip, buf + pos, size);
        break;
      case vault_export_write_fileinfo_close:
        zip_entry_close(zip);
        break;
      case vault_export_write_presetinfo:
        if (packageType == package_type_preset) {
          size = snprintf(buf, bufsize, PRESET_INFO_FILE_CONTENT, packageName, packageName);
          zip_entry_open(zip, presetInfoFileName);
          zip_entry_write(zip, buf, size);
          zip_entry_close(zip);
        }
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
        else if (packageType == package_type_lib) {
          zip_entry_open(zip, favoriteFileName);
          zip_entry_write(zip, favoriteFileContent, strlen(favoriteFileContent));
          zip_entry_close(zip);
        }
#endif
        resourceType = 0;
        break;
      case vault_export_dir_open:
        dir.path = resourcePath[resourceType];
        dir.refresh(resourceFileExtension[resourceType]);
        resourceIndex = 0;
        break;
      case vault_export_write_infofile:
        getInfoFileName(path, resourceIndex, resourceType);
        zip_entry_open(zip, path);
        zip_entry_write(zip, resourceInfoFileContent[resourceType], strlen(resourceInfoFileContent[resourceType]));
//  zip_entry_write(zip, &dir.count, sizeof(dir.count));
//  zip_entry_write(zip, &resourceIndex, sizeof(resourceIndex));
//  zip_entry_write(zip, resourcePath[resourceType], strlen(resourcePath[resourceType]));
//  zip_entry_write(zip, resourceFileExtension[resourceType], strlen(resourceFileExtension[resourceType]));
//  if (resourceIndex < dir.count)
//    zip_entry_write(zip, dir.get(resourceIndex), strlen(dir.get(resourceIndex)));
        zip_entry_close(zip);
        break;
      case vault_export_read_binfile:
        src = resourceInitData[resourceType];         
        size = resourceInitDataSize[resourceType];
        dir_index = resourceIndex + offset;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//correct mk2 MODERN/FUTURE banks sorting
        if (dir_index >= BANK_SIZE && dir_index < BANK_SIZE * 2)
          dir_index += BANK_SIZE;
        else if (dir_index >= BANK_SIZE * 2 && dir_index < BANK_SIZE * 3)
          dir_index -= BANK_SIZE;
#endif      
        if (dir_index >= dir.count)
          break;
        snprintf(path, MAXNAMLEN + 1, "%s/%s", resourcePath[resourceType], dir.get(dir_index));
        if ((fp = fopen(path, "rb")) == NULL
          || fstat(fileno(fp), &st) != 0
          || st.st_size != (int)fread(buf, 1, st.st_size, fp)
          || fclose(fp) != 0
        )
          break;
        src = (unsigned char*)buf + RESOURCE_HEADER_SIZE;
        memcpy(&size, buf + RESOURCE_HEADER_SIZE - RESOURCE_HEADER_LENGTH_SIZE, RESOURCE_HEADER_LENGTH_SIZE);
        break;
      case vault_export_open_binfile_entry:
        getBinFileName(path, resourceIndex, resourceType);
        zip_entry_open(zip, path);
        pos = 0;
        break;
      case vault_export_write_binfile_next_chunk:
        if ((int)size < VAULT_CHUNK_SIZE)
          break;
        zip_entry_write(zip, src + pos, VAULT_CHUNK_SIZE);
        size -= VAULT_CHUNK_SIZE;
        pos += VAULT_CHUNK_SIZE;
        break;
      case vault_export_write_binfile_last_chunk:
        if ((int)size > 0)
          zip_entry_write(zip, src + pos, size);
        break;
      case vault_export_close_binfile_entry:
        zip_entry_close(zip);
        break;
      case vault_export_entry_next:
        if (++resourceIndex < resourceTypeCount[resourceType])
          return state = vault_export_write_infofile;
        else if (++resourceType < RESOURCE_TYPE_COUNT)
          return state = vault_export_dir_open;
        break;
      case vault_import_package_open:
        zip = zip_open(path, 0, 'r');
        resourceType = 0;
        break;
      case vault_import_dir_open:
        headerCrc = crc32(0, (const mz_uint8*)resourceFileHeader[resourceType], RESOURCE_HEADER_SIZE - RESOURCE_HEADER_LENGTH_SIZE);
        dir.path = resourcePath[resourceType];
        dir.refresh(resourceFileExtension[resourceType]);
        resourceIndex = 0;
        break;
      case vault_import_entry_open:
        if (zip_entry_open(zip, getBinFileName(path, resourceIndex, resourceType)) < 0)
          return state = vault_import_entry_next;
        break;
      case vault_import_entry_read_first_chunk:
        pos = 0;
        size = zip_entry_size(zip);
        firstchunksize = size < VAULT_CHUNK_SIZE ? size : VAULT_CHUNK_SIZE;
        zip_entry_noallocreadwithoffset(zip, pos, firstchunksize, buf);
        crc = crc32(headerCrc, (const mz_uint8*)&size, RESOURCE_HEADER_LENGTH_SIZE);
        crc = crc32(crc, (const mz_uint8*)buf, firstchunksize);
        dir_index = resourceIndex + offset;
        resourceFileName = getResourceFileName(buf, dir_index, resourceType);
        snprintf(path, MAXNAMLEN + 1, "%s/%s", resourcePath[resourceType], resourceFileName);
        break;
      case vault_import_entry_file_open:
#ifndef VAULT_WRITE_DISABLE
printf("%d %d %s\n", resourceType, resourceIndex, path);
        if ((fp = fopen(path, "wb")) != NULL)
          break;
        zip_entry_close(zip);
        return state = vault_import_entry_next;
#endif
        break;
      case vault_import_entry_write_1st_chunk:
#ifndef VAULT_WRITE_DISABLE
        fwrite(resourceFileHeader[resourceType], 1, RESOURCE_HEADER_SIZE - RESOURCE_HEADER_LENGTH_SIZE, fp);
        fwrite(&size, 1, RESOURCE_HEADER_LENGTH_SIZE, fp);
        fwrite(buf, 1, firstchunksize, fp);
#endif
        size -= firstchunksize;
        pos += firstchunksize;
        break;
      case vault_import_entry_process_next_chunk:
        if ((int)size < VAULT_CHUNK_SIZE)
          break;
        zip_entry_noallocreadwithoffset(zip, pos, VAULT_CHUNK_SIZE, buf);
        crc = crc32(crc, (const mz_uint8*)buf, VAULT_CHUNK_SIZE);
#ifndef VAULT_WRITE_DISABLE
        fwrite(buf, 1, VAULT_CHUNK_SIZE, fp);
#endif
        size -= VAULT_CHUNK_SIZE;
        pos += VAULT_CHUNK_SIZE;
        return state;
      case vault_import_entry_process_last_chunk:
        if ((int)size > 0) {
          zip_entry_noallocreadwithoffset(zip, pos, size, buf);
          crc = crc32(crc, (const mz_uint8*)buf, size);
#ifndef VAULT_WRITE_DISABLE
          fwrite(buf, 1, size, fp);
#endif
        }
#ifndef VAULT_WRITE_DISABLE
        fwrite(&crc, 1, RESOURCE_HEADER_CRC_SIZE, fp);
#endif
        break;
      case vault_import_entry_close:
        zip_entry_close(zip);
#ifndef VAULT_WRITE_DISABLE
        fclose(fp);
#endif
        break;
      case vault_import_entry_remove_duplicate:
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//correct mk2 MODERN/FUTURE banks sorting
        if (dir_index >= BANK_SIZE && dir_index < BANK_SIZE * 2)
          dir_index += BANK_SIZE;
        else if (dir_index >= BANK_SIZE * 2 && dir_index < BANK_SIZE * 3)
          dir_index -= BANK_SIZE;
#endif      
        if (dir_index < dir.count && strcmp(dir.get(dir_index), resourceFileName)) {
          snprintf(path, MAXNAMLEN + 1, "%s/%s", resourcePath[resourceType], dir.get(dir_index));
          remove(path);
          dir.remove(dir_index);
        }
        break;
      case vault_import_entry_next:
        if (++resourceIndex < resourceTypeCount[resourceType])
          return state = vault_import_entry_open;
        else if (++resourceType < RESOURCE_TYPE_COUNT)
          return state = vault_import_dir_open;
        break;
      case vault_export_package_close:
      case vault_import_package_close:
        if (path != nullptr)
          free(path);
        path = nullptr;
        if (buf != nullptr)
          free(buf);
        buf = nullptr;
        zip_close(zip);
        break;
      case vault_export_finished:
      case vault_import_finished:
      default:
        return state = vault_idle;
    }  
    return ++state;
  }
};
