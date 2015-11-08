/*
 *      Copyright (C) 2015 MrMC
 *      https://github.com/MrMC
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MrMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */


#include "zlib.h"
#include "zconf.h"

#include "SubtitleUtilities.h"
#include "filesystem/File.h"
#include "utils/Base64.h"
#include "utils/StringUtils.h"

#include <openssl/sha.h>

bool CSubtitleUtilities::SubtitleFileSizeAndHash(const std::string &path, std::string &strSize, std::string &strHash)
{
  
  const size_t chksum_block_size = 8192;
  XFILE::CFile file;
  size_t i;
  uint64_t hash = 0;
  uint64_t buffer1[chksum_block_size*2];
  uint64_t fileSize ;
  // In natural language it calculates: size + 64k chksum of the first and last 64k
  // (even if they overlap because the file is smaller than 128k).
  file.Open(path, READ_NO_CACHE); //open file
  file.Read(buffer1, chksum_block_size*sizeof(uint64_t)); //read first 64k
  file.Seek(-(int64_t)chksum_block_size*sizeof(uint64_t), SEEK_END); //seek to the end of the file
  file.Read(&buffer1[chksum_block_size], chksum_block_size*sizeof(uint64_t)); //read last 64k
  
  for (i=0;i<chksum_block_size*2;i++)
    hash += buffer1[i];
  
  fileSize = file.GetLength();
  
  hash += fileSize; //add size
  
  file.Close(); //close file
  
  strHash = StringUtils::Format("%" PRIx64"", hash);     //format hash
  strSize = StringUtils::Format("%llu", fileSize); // format size
  return true;
  
}

// below from http://windrealm.org/tutorials/decompress-gzip-stream.php
bool CSubtitleUtilities::gzipInflate( const std::string& compressedBytes, std::string& uncompressedBytes )
{
  if ( compressedBytes.size() == 0 ) {
    uncompressedBytes = compressedBytes ;
    return true ;
  }
  
  uncompressedBytes.clear() ;
  
  unsigned full_length = compressedBytes.size() ;
  unsigned half_length = compressedBytes.size() / 2;
  
  unsigned uncompLength = full_length ;
  char* uncomp = (char*) calloc( sizeof(char), uncompLength );
  
  z_stream strm;
  strm.next_in = (Bytef *) compressedBytes.c_str();
  strm.avail_in = compressedBytes.size() ;
  strm.total_out = 0;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  
  bool done = false ;
  
  if (inflateInit2(&strm, (16+MAX_WBITS)) != Z_OK) {
    free( uncomp );
    return false;
  }
  
  while (!done) {
    // If our output buffer is too small
    if (strm.total_out >= uncompLength ) {
      // Increase size of output buffer
      char* uncomp2 = (char*) calloc( sizeof(char), uncompLength + half_length );
      memcpy( uncomp2, uncomp, uncompLength );
      uncompLength += half_length ;
      free( uncomp );
      uncomp = uncomp2 ;
    }
    
    strm.next_out = (Bytef *) (uncomp + strm.total_out);
    strm.avail_out = uncompLength - strm.total_out;
    
    // Inflate another chunk.
    int err = inflate (&strm, Z_SYNC_FLUSH);
    if (err == Z_STREAM_END) done = true;
    else if (err != Z_OK)  {
      break;
    }
  }
  
  if (inflateEnd (&strm) != Z_OK) {
    free( uncomp );
    return false;
  }
  
  for ( size_t i=0; i<strm.total_out; ++i ) {
    uncompressedBytes += uncomp[ i ];
  }
  free( uncomp );
  return true ;
}


std::string CSubtitleUtilities::sha256(const std::string *string)
{
  char outputBuffer[65];
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, string, string->size());
  SHA256_Final(hash, &sha256);
  int i = 0;
  for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
  {
    sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
  }
  outputBuffer[64] = 0;
  return outputBuffer;
}

