/* vim:ts=4:sw=4:et
 *
 * Some parts taken from dvd_udf.c
 *
 * Miroslav Jezbera <[EMAIL PROTECTED]>
 *
 * dvd_iso9660: parse and read the ISO9660 filesystem (hybrid ISO+UDF DVD disks)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  Or, point your browser to
 * http://www.gnu.org/copyleft/gpl.html
 */

//#include "libDVD/src/reader/config/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include "dvd_reader.h"
#include "dvd_iso9660.h"

/* Private but located in/shared with dvd_reader.c */
extern int InternalUDFReadBlocksRaw( dvd_reader_t *device, uint32_t lb_number,
				size_t block_count, unsigned char *data,
				int encrypted );

/* Macros */
#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN) && !defined(WORDS_BIGENDIAN)
#   define LITTLE_ENDIAN
#endif /* LITTLE_ENDIAN */

/*
#define ISO_DEBUG 1
*/

#define FAILURE   0
#define SUCCESS   1

#define ISO_SECTOR_SIZE         DVD_VIDEO_LB_LEN
#define ISO_TBL_REC_NAME_MAX    31
#define ISO_MAX_PATH_NESTING    8

#define ISO_FIRST_VOL_DESC_SECTOR   16
#define ISO_LAST_VOL_DESC_SECTOR    17
#define ISO_VOL_DESC_HEADER         "\01CD001\01"

#define READ_WORD_LE(ptr)  (uint16_t)(((uint8_t *)ptr)[0] | ((uint8_t *)ptr)[1] << 8)
#define READ_DWORD_LE(ptr) (uint32_t)(((uint8_t *)ptr)[0] | ((uint8_t *)ptr)[1] << 8 | \
                            ((uint8_t *)ptr)[2] << 16 | ((uint8_t *)ptr)[3] << 24)

#define IS_DIRECTORY(flags)   (((flags)&2)!=0)

#ifdef LITTLE_ENDIAN
#  define ISO_DIR_REC_FILE_POS   2
#  define ISO_DIR_REC_FILE_SIZE 10
#else /* !LITTLE_ENDIAN */
#  define ISO_DIR_REC_FILE_POS   6
#  define ISO_DIR_REC_FILE_SIZE 14
#endif /* !LITTLE_ENDIAN */
#define ISO_DIR_REC_FILE_FLAGS  25
#define ISO_DIR_REC_NAME_LEN    32

/* Types */
typedef uint8_t iso_sector_t[ISO_SECTOR_SIZE];

struct iso_volume_desc_t
   {
   char mSysId[33];
   char mVolId[33];
   uint32_t mSectorsCnt;
   uint32_t mPathTblLen;
   uint32_t mFirstPathTblStart;
   uint32_t mSecondPathTblStart;
   };

struct iso_path_table_record_t
   {
   uint8_t  mRecordIdx;
   uint8_t  mNameLen;
   uint32_t mDirectoryFirstSector;
   uint16_t mParentDirRecordIdx;
   char     mName[ISO_TBL_REC_NAME_MAX+1];
   struct iso_path_table_record_t *next;
   };

/* Common functions */
char *Rtrim(char *str)
   {
   char *ptr;
   if (str == NULL) return NULL;
   ptr = str + strlen(str) - 1;
   while (ptr >= str && isspace(ptr[0])) --ptr;
   ptr[1] = '\0';
   return str;
   }

/* Functions for ISO FS */
int
iso_read_volume_descriptor(dvd_reader_t *aDevice, struct iso_volume_desc_t *aVolumeDesc)
    {
    iso_sector_t content;
    uint32_t sector;
    int result = FAILURE;

    for (sector = ISO_FIRST_VOL_DESC_SECTOR; sector <= ISO_LAST_VOL_DESC_SECTOR; ++sector)
        {
        if (InternalUDFReadBlocksRaw(aDevice, sector, 1, content, 0) <= 0) continue;
        if (strcmp((char *)content, ISO_VOL_DESC_HEADER) == 0)
            {
            result = SUCCESS;
            break;
            }
        }
    if (result != SUCCESS) return result;
    memset((void *)aVolumeDesc, 0, sizeof(struct iso_volume_desc_t));
    memcpy((void *)aVolumeDesc->mSysId, (void *)&content[8], 32);
    memcpy((void *)aVolumeDesc->mVolId, (void *)&content[40], 32);
    Rtrim((char *)aVolumeDesc->mSysId);
    Rtrim((char *)aVolumeDesc->mVolId);
#ifdef LITTLE_ENDIAN
    aVolumeDesc->mSectorsCnt = *(uint32_t *)&content[80];
    assert(*(uint16_t *)&content[128] == 2048);
    aVolumeDesc->mPathTblLen = *(uint32_t *)&content[132];
    aVolumeDesc->mFirstPathTblStart = *(uint32_t *)&content[140];
    aVolumeDesc->mSecondPathTblStart = *(uint32_t *)&content[144];
#else /* !LITTLE_ENDIAN */
    aVolumeDesc->mSectorsCnt = *(uint32_t *)&content[84];
    assert(*(uint16_t *)&content[130] == 2048);
    aVolumeDesc->mPathTblLen = *(uint32_t *)&content[136];
    aVolumeDesc->mFirstPathTblStart = *(uint32_t *)&content[148];
    aVolumeDesc->mSecondPathTblStart = *(uint32_t *)&content[152];
#endif /* !LITTLE_ENDIAN */
    return SUCCESS;
    }

void
iso_path_table_free(struct iso_path_table_record_t *list)
    {
    struct iso_path_table_record_t *ptr;
    while (list != NULL)
        {
        ptr = list;
        list = list->next;
        free(ptr);
        }
    }

int
iso_read_path_table(dvd_reader_t *aDevice, struct iso_volume_desc_t *aVolumeDesc, struct iso_path_table_record_t **aRecords)
    {
    uint8_t *content, *ptr;
    uint32_t sector, last_sector;
    struct iso_path_table_record_t head, *current = &head;
    uint8_t recordIdx = 0;

    assert(aRecords != NULL);
    if (aVolumeDesc->mPathTblLen == 0) return FAILURE;
    sector = aVolumeDesc->mFirstPathTblStart;
    last_sector = sector + (aVolumeDesc->mPathTblLen - 1) / ISO_SECTOR_SIZE;
    content = (uint8_t *)malloc(ISO_SECTOR_SIZE * (last_sector - sector + 1));
    if (content == NULL) return FAILURE;
    for (ptr = content; sector <= last_sector; ++sector, ptr += ISO_SECTOR_SIZE)
        {
        if (InternalUDFReadBlocksRaw(aDevice, sector, 1, ptr, 0) <= 0)
            {
            free(content);
            return FAILURE;
            }
        }
    for (ptr = content; ptr < content + aVolumeDesc->mPathTblLen;)
        {
        current->next = (struct iso_path_table_record_t *)malloc(sizeof(struct iso_path_table_record_t));
        if (current->next == NULL)
            {
            free(content);
            iso_path_table_free(head.next);
            return FAILURE;
            }
        current = current->next;
        memset((void *)current, 0, sizeof(struct iso_path_table_record_t));
        current->mRecordIdx = ++recordIdx;
        current->mNameLen = *ptr;
        assert(current->mNameLen > 0 && current->mNameLen <= ISO_TBL_REC_NAME_MAX);
        current->mDirectoryFirstSector = READ_DWORD_LE(ptr + 2);
        current->mParentDirRecordIdx = READ_WORD_LE(ptr + 6);
        memcpy((void *)current->mName, ptr + 8, current->mNameLen);
        ptr += 8 + ((current->mNameLen + 1) & ~1);
#ifdef ISO_DEBUG
        fprintf(stderr,
                "Name length:             %u\n"
                "Directory first sector:  %u\n"
                "Parent directory record: %hu\n"
                "Name:                    \"%s\"\n"
                , (int)current->mNameLen, current->mDirectoryFirstSector,
                current->mParentDirRecordIdx, current->mName);
#endif /* ISO_DEBUG */
        }
    assert(content + aVolumeDesc->mPathTblLen == ptr);
    free(content);
    *aRecords = head.next;

    return SUCCESS;
    }

uint32_t
iso_search_directory(dvd_reader_t *aDevice, uint32_t aDirectorySector, char *aFilename, uint32_t *aFileSize)
    {
    uint32_t directorySize = ISO_SECTOR_SIZE, position, filePosition, fileSize;
    uint8_t recordSize, identifierSize, flags;
    iso_sector_t content;
    char identifier[256];
    int filenameLen;

    *aFileSize = 0;
    filenameLen = strlen(aFilename);
    if (InternalUDFReadBlocksRaw(aDevice, aDirectorySector, 1, content, 0) <= 0)
        {
        return 0;
        }
    for (position = 0; position < directorySize; position += recordSize)
        {
        if (position >= ISO_SECTOR_SIZE || content[position] == 0)
            {
            if (directorySize <= ISO_SECTOR_SIZE) return 0;
            if (InternalUDFReadBlocksRaw(aDevice, ++aDirectorySector, 1, content, 0) <= 0)
                {
                return 0;
                }
            position = 0;
            directorySize -= ISO_SECTOR_SIZE;
            }
        recordSize = content[position];
        if (recordSize == 0) break;
        filePosition = READ_DWORD_LE(&content[position + ISO_DIR_REC_FILE_POS]);
        fileSize = READ_DWORD_LE(&content[position + ISO_DIR_REC_FILE_SIZE]);
        flags = content[position + ISO_DIR_REC_FILE_FLAGS];
        identifierSize = content[position + ISO_DIR_REC_NAME_LEN];
        memcpy((void *)identifier, (void *)&content[position + ISO_DIR_REC_NAME_LEN + 1], identifierSize);
        identifier[identifierSize] = '\0';
        if (identifierSize == 1 && identifier[0] == 0) /* current directory */
            {
            assert(IS_DIRECTORY(flags));
            directorySize = fileSize;
            }
#ifdef ISO_DEBUG
        fprintf(stderr,
                "record size:   %u\n"
                "file position: %u\n"
                "file size:     %u\n"
                "directory:     %d\n"
                "name size:     %u\n"
                "name:          \"%s\"\n"
                , (int)recordSize, filePosition, fileSize, (int)IS_DIRECTORY(flags),
                (int)identifierSize, identifier);
#endif /* ISO_DEBUG */
        if (!IS_DIRECTORY(flags) && (filenameLen == identifierSize ||
                    (filenameLen < identifierSize && identifier[filenameLen] == ';')) &&
                strncasecmp(aFilename, identifier, filenameLen) == 0)
            {
            *aFileSize = fileSize;
            return filePosition;
            }
        }

    return 0;
    }

/* Public interface */
uint32_t ISOFindFile(dvd_reader_t *aDevice, const char *aFilename, uint32_t *aFileSize)
    {
    struct iso_volume_desc_t volumeDesc;
    struct iso_path_table_record_t *directories, *current;
    char *path, *rest, *part, *file, last;
    int parentIdx;
    uint32_t directoryPosition;

    *aFileSize = 0;
    if (aFilename == NULL || aFilename[0] == '\0') return 0;
    if (iso_read_volume_descriptor(aDevice, &volumeDesc) != SUCCESS)
        {
        return 0;
        }
#ifdef ISO_DEBUG
    fprintf(stderr,
            "System identifier:  \"%s\"\n"
            "Volume identifier:  \"%s\"\n"
            "Sectors count:      %u\n"
            "Path table length:  %u\n"
            "First path table:   %u\n"
            "Second path table:  %u\n"
            , volumeDesc.mSysId, volumeDesc.mVolId, volumeDesc.mSectorsCnt, volumeDesc.mPathTblLen,
            volumeDesc.mFirstPathTblStart, volumeDesc.mSecondPathTblStart);
#endif /* ISO_DEBUG */
    if (iso_read_path_table(aDevice, &volumeDesc, &directories) != SUCCESS)
        {
        return 0;
        }

    parentIdx = 1; /* root directory */
    current = directories;
    path = rest = strdup(aFilename);
    file = strrchr(path, '/');
    if (file != NULL)
        {
        *file = '\0';
        file = (char*)aFilename + (file - path + 1);
        last = '/';
        }
    else
        {
        file = path;
        last = '\0';
        }
    while (last != '\0' && *rest != '\0' && current != NULL)
        {
        while (*rest == '/') rest++;
        part = rest;
        while (*rest != '/' && *rest != '\0') rest++;
        last = *rest;
        *rest++ = '\0';
        /* compare part of path with ISO path table entries */
        while (current != NULL && (parentIdx != current->mParentDirRecordIdx ||
                    strcasecmp(current->mName, part) != 0)) current = current->next;
        if (current != NULL)
            {
            parentIdx = current->mRecordIdx;
            }
        }

    if (current == NULL)
        {
        iso_path_table_free(directories);
        free(path);
        return 0;
        }
    directoryPosition = current->mDirectoryFirstSector;
    iso_path_table_free(directories);
    free(path);
    return iso_search_directory(aDevice, directoryPosition, file, aFileSize);
    }
