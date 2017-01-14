/*
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
#ifndef DVD_ISO9660_H
#define DVD_ISO9660_H 1

#include <inttypes.h>

#include "dvd_reader.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Looks for a file on the hybrid ISO9660/mini-UDF disc/imagefile and returns
 * the block number where it begins, or 0 if it is not found.  The filename
 * should be an absolute pathname on the UDF filesystem, starting with '/'.
 * For example '/VIDEO_TS/VTS_01_1.IFO'.  On success, filesize will be set to
 * the size of the file in bytes.
 */
uint32_t ISOFindFile(dvd_reader_t *aDevice, const char *aFilename, uint32_t *aFileSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DVD_ISO9660_H */
