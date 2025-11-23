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

#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
#include "init_drmlg.h"
#define PRODUCT_NAME "drumlogue"
#define PRODUCT_CODE "drmlg"
#define PACKAGE_PATH "/var/lib/drumlogued/userfs/Programs"
#define PACKAGE_NAME_TEMPLATE "%s_%c_%d"
#define RESOURCE_TYPE_COUNT 2
#define RESOURCE_PATH "/var/lib/drumlogued/userfs/Programs", "/var/lib/drumlogued/userfs/Kits"
#define RESOURCE_INFO_FILE_CONTENT "<" PRODUCT_NAME "_ProgramInformation/>", "<" PRODUCT_NAME "_KitInformation/>"
#define RESOURCE_INFO_FILE_NAME_TEMPLATE "Prog_%03d.prog_info", "Kit_%03d.kit_info"
#define RESOURCE_BIN_FILE_NAME_TEMPLATES "Prog_%03d.prog_bin", "Kit_%03d.kit_bin"
#define RESOURCE_BIN_FILE_EXTENSION ".prog_bin", ".kit_bin"
#define RESOURCE_FILE_PREFIX "%c%02d_"
#define RESOURCE_FILE_EXTENSION "." PRODUCT_CODE "prog", "." PRODUCT_CODE "kit"
#define RESOURCE_NAME_OFFSET 20, 20
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
  "    </KitData>\n" \

#define BANK_SIZE 16
#define GENRE_SIZE 0
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
#include "init_mk2.h"
#define PRODUCT_NAME "microKORG2"
#define PRODUCT_CODE "mk2"
#define PACKAGE_PATH "/var/lib/microkorgd/userfs/Programs"
#define PACKAGE_NAME_TEMPLATE "%s_%c_%d"
#define RESOURCE_TYPE_COUNT 1
#define RESOURCE_PATH "/var/lib/microkorgd/userfs/Programs"
#define RESOURCE_INFO_FILE_CONTENT "<" PRODUCT_NAME "_ProgramInformation/>"
#define RESOURCE_INFO_FILE_NAME_TEMPLATE "Prog_%03d.prog_info"
#define RESOURCE_BIN_FILE_NAME_TEMPLATES "Prog_%03d.prog_bin"
#define RESOURCE_BIN_FILE_EXTENSION ".prog_bin"
#define RESOURCE_FILE_PREFIX "%c_%c%d_"
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

#define PRESET_INFO_FILE_CONTENT \
  "<" PRODUCT_NAME "_Preset>\n" \
  "  <DataID>%s</DataID>\n" \
  "  <Name>%s</Name>\n" \
  "</" PRODUCT_NAME "_Preset>\n"

//DEBUG
#undef PACKAGE_PATH
#define PACKAGE_PATH "./" PRODUCT_CODE "/Programs"
#undef RESOURCE_PATH
#define RESOURCE_PATH "./" PRODUCT_CODE "/Programs", "./" PRODUCT_CODE "/Kits"
//DEBUG

static const char *fileInfoFileName = "FileInformation.xml";
static const char *presetInfoFileName = "PresetInformation.xml";
static const char *resourcePath[] = {RESOURCE_PATH};
static const char *resourceInfoFileNameTemplate[] = {RESOURCE_INFO_FILE_NAME_TEMPLATE};
static const char *resourceBinFileNameTemplate[] = {RESOURCE_BIN_FILE_NAME_TEMPLATES};
static const char *contentMetaTemplates[] = {CONTENT_META_TEMPLATES};
static const char *contentDataTemplates[] = {CONTENT_DATA_TEMPLATES};
static const char *resourceInfoFileContent[] = {RESOURCE_INFO_FILE_CONTENT};
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
/*
static int getResourceIndex(char *name) {
  char bank, genre;
  int index;
#if defined(UNIT_TARGET_PLATFORM_DRUMLOGUE)
    sscanf(name, RESOURCE_FILE_PREFIX, &bank, &index);
    bank -= 'A';
#elif defined(UNIT_TARGET_PLATFORM_MICROKORG2)
    sscanf(name, RESOURCE_FILE_PREFIX, &bank, &genre, &index);
    bank = strchr(bankLetters, bank) - bankLetters;
    genre -= 'A';
#endif
  return bank * BANK_SIZE + genre * GENRE_SIZE + index - 1;
}
*/
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

  if (packageType == package_type_preset)
    pos += snprintf(buf + pos, bufsize - pos,
      "    <PresetInformation>\n"
      "      <File>%s</File>\n"
      "    </PresetInformation>\n",
      presetInfoFileName);

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

size_t vault_export(const char *packageName, int packageType, int *resourceTypeCount, int offset) {
  size_t total_read = 0;
  const size_t bufsize = 65536;
  size_t size;

  char *buf = (char*)malloc(bufsize);
  if (!buf)
    return total_read;

  const void *src;
  char path[MAXNAMLEN + 1];
  FILE *fp;
  struct stat st;
  struct zip_t *zip = zip_stream_open(NULL, 0, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');

  size = getFileInfoContent(resourceTypeCount, packageType, buf, bufsize);
  zip_entry_open(zip, fileInfoFileName);
  zip_entry_write(zip, buf, size);
  zip_entry_close(zip);

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

  for (int resourceType = 0; resourceType < RESOURCE_TYPE_COUNT; resourceType++) {
    fs_dir dir = fs_dir(resourcePath[resourceType], resourceFileExtension[resourceType]);
    for (int index = 0; index < resourceTypeCount[resourceType]; index++) {
      getInfoFileName(path, index, resourceType);
      zip_entry_open(zip, path);
      zip_entry_write(zip, resourceInfoFileContent[resourceType], strlen(resourceInfoFileContent[resourceType]));
      zip_entry_close(zip);

      src = resourceInitData[resourceType];         
      size = resourceInitDataSize[resourceType];
      int dir_index = index + offset;
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//correct mk2 MODERN/FUTURE banks sorting
        if (dir_index >= BANK_SIZE && dir_index < BANK_SIZE * 2)
          dir_index += BANK_SIZE;
        else if (dir_index >= BANK_SIZE * 2 && dir_index < BANK_SIZE * 3)
          dir_index -= BANK_SIZE;
#endif      
      if (dir_index < dir.count) {
        snprintf(path, sizeof(path), "%s/%s", resourcePath[resourceType], dir.names[dir_index]);
        if ((fp = fopen(path, "rb")) != NULL
          && fstat(fileno(fp), &st) == 0
          && st.st_size == (int)fread(buf, 1, st.st_size, fp)
          && fclose(fp) == 0
        ) {
          src = buf;
          size = st.st_size;
        }
      }
      getBinFileName(path, index, resourceType);
      zip_entry_open(zip, path);
      zip_entry_write(zip, src, size);
      zip_entry_close(zip);
      total_read += size;
    }
  }

  free(buf);
  snprintf(path, sizeof(path), "%s/%s%s", packagePath, packageName, packageExtension[packageType]);
  fp = fopen(path, "wb");
  if (!fp)
    return total_read;
  zip_stream_copy(zip, (void **)&buf, &size);
  zip_stream_close(zip);
  fwrite(buf, 1, size, fp);
  fclose(fp);  
  free(buf);
  return total_read;
}

size_t vault_import(const char *packageName, int packageType, int *resourceTypeCount, int offset) {
  size_t total_write = 0;
  char *buf = NULL;
  size_t bufsize = 0, size;
  char path[MAXNAMLEN + 1];

  snprintf(path, sizeof(path), "%s/%s%s", packagePath, packageName, packageExtension[packageType]);
  struct zip_t *zip = zip_open(path, 0, 'r');
  for (int resourceType = 0; resourceType < RESOURCE_TYPE_COUNT; resourceType++) {
    fs_dir dir = fs_dir(resourcePath[resourceType], resourceFileExtension[resourceType]);
    for (int index = 0; index < resourceTypeCount[resourceType]; ++index) {
      if (zip_entry_open(zip, getBinFileName(path, index, resourceType)) < 0)
        continue;
      size = zip_entry_size(zip);
      if (!buf || bufsize < size) {
          free(buf);
          buf = (char*)malloc(size);
          bufsize = size;
      }
      zip_entry_noallocread(zip, (void *)buf, bufsize);
      int dir_index = index + offset;
      char *resourceFileName = getResourceFileName(buf, dir_index, resourceType);
      snprintf(path, sizeof(path), "%s/%s", resourcePath[resourceType], resourceFileName);
      FILE *fp = fopen(path, "wb");
      if (fp) {
        total_write += fwrite(buf, 1, size, fp);
        fclose(fp);
#ifdef UNIT_TARGET_PLATFORM_MICROKORG2
//correct mk2 MODERN/FUTURE banks sorting
        if (dir_index >= BANK_SIZE && dir_index < BANK_SIZE * 2)
          dir_index += BANK_SIZE;
        else if (dir_index >= BANK_SIZE * 2 && dir_index < BANK_SIZE * 3)
          dir_index -= BANK_SIZE;
#endif      
        if (dir_index < dir.count && strcmp(dir.names[dir_index], resourceFileName)) {
          snprintf(path, sizeof(path), "%s/%s", resourcePath[resourceType], dir.names[dir_index]);
          remove(path);
          dir.remove(dir_index);
        }
      }
      zip_entry_close(zip);
    }
  }
  free(buf);
  zip_close(zip);
  return total_write;
}
