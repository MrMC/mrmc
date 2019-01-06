/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is MPEG4IP.
 * 
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2001-2002.  All Rights Reserved.
 * 
 * Contributor(s): 
 *		Dave Mackie		dmackie@cisco.com
 */

#include <assert.h>
#include <ctype.h> /* isdigit, isprint, isspace */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>

#ifndef __MBS_INCLUDED__
#define __MBS_INCLUDED__ 

class CMemoryBitstream {
public:
	CMemoryBitstream() {
		m_pBuf = NULL;
		m_bitPos = 0;
		m_numBits = 0;
		m_alloced = false;
	}
	~CMemoryBitstream(void) {
	  if (m_alloced) free(m_pBuf);
	}
	void AllocBytes(u_int32_t numBytes);

	void SetBytes(u_int8_t* pBytes, u_int32_t numBytes);

	void PutBytes(u_int8_t* pBytes, u_int32_t numBytes);

	void PutBits(u_int32_t bits, u_int32_t numBits);

    u_int32_t PeekBits(u_int32_t numBits);
    u_int32_t GetBits(u_int32_t numBits);

	void SkipBytes(u_int32_t numBytes) {
		SkipBits(numBytes << 3);
	}

	void SkipBits(u_int32_t numBits) {
		SetBitPosition(GetBitPosition() + numBits);
	}

	u_int32_t GetBitPosition() {
		return m_bitPos;
	}

	void SetBitPosition(u_int32_t bitPos) {
		if (bitPos > m_numBits) {
			throw EIO;
		}
		m_bitPos = bitPos;
	}

	u_int8_t* GetBuffer() {
	  m_alloced = false;
		return m_pBuf;
	}

	u_int32_t GetNumberOfBytes() {
		return (GetNumberOfBits() + 7) / 8;
	}

	u_int32_t GetNumberOfBits() {
		return m_numBits;
	}

    u_int32_t GetRemainingBits() {
        return m_numBits - m_bitPos;
    }


protected:
	u_int8_t*	m_pBuf;
	u_int32_t	m_bitPos;
	u_int32_t	m_numBits;
	bool m_alloced;
};

#endif /* __MBS_INCLUDED__ */ 

