/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 * Copyright (c) 2014-2016 Valve Corporation. All rights reserved.
 * Copyright (C) 2014-2016 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Jon Ashburn <jon@lunarg.com>
 * Author: Peter Lohrmann <peterl@valvesoftware.com>
 */

#include "vktrace_filelike.h"
#include "vktrace_common.h"
#include "vktrace_interconnect.h"
#include <assert.h>
#include <stdlib.h>

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Checkpoint* vktrace_Checkpoint_create(const char* _str) {
    Checkpoint* pCheckpoint = VKTRACE_NEW(Checkpoint);
    pCheckpoint->mToken = _str;
    pCheckpoint->mTokenLength = strlen(_str) + 1;
    return pCheckpoint;
}

// ------------------------------------------------------------------------------------------------
void vktrace_Checkpoint_write(Checkpoint* pCheckpoint, FileLike* _out) {
    vktrace_FileLike_Write(_out, pCheckpoint->mToken, pCheckpoint->mTokenLength);
}

// ------------------------------------------------------------------------------------------------
BOOL vktrace_Checkpoint_read(Checkpoint* pCheckpoint, FileLike* _in) {
    if (pCheckpoint->mTokenLength < 64) {
        char buffer[64];
        vktrace_FileLike_Read(_in, buffer, pCheckpoint->mTokenLength);
        if (strcmp(buffer, pCheckpoint->mToken) != 0) {
            return FALSE;
        }
    } else {
        char* buffer = VKTRACE_NEW_ARRAY(char, pCheckpoint->mTokenLength);
        vktrace_FileLike_Read(_in, buffer, pCheckpoint->mTokenLength);
        if (strcmp(buffer, pCheckpoint->mToken) != 0) {
            VKTRACE_DELETE(buffer);
            return FALSE;
        }
        VKTRACE_DELETE(buffer);
    }
    return TRUE;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
uint64_t vktrace_FileLike_GetFileLength(FILE* fp) {
    uint64_t byte_length = 0;

    // Get file length
    int64_t length = 0;
    if (Fseek(fp, 0, SEEK_END) != 0) {
        vktrace_LogError("Failed to Fseek to the end of tracefile for replaying.");
    } else {
        length = Ftell(fp);
        if (length == -1L) {
            vktrace_LogError("Failed to get the length of tracefile for replaying.");
            length = 0;
        }
    }

    // WARNING: Reset file position to the beginning of the file
    // Because this function is only called from vktrace_FileLike_create_file,
    // the file position should always be the beginning of the file before getting file length.
    rewind(fp);

    byte_length = length;
    return byte_length;
}

// ------------------------------------------------------------------------------------------------
FileLike* vktrace_FileLike_create_file(FILE* fp, BOOL preload) {
    FileLike* pFile = NULL;
    if (fp != NULL) {
        pFile = VKTRACE_NEW(FileLike);
        pFile->mFile = NULL;
        pFile->mFileLen = vktrace_FileLike_GetFileLength(fp);
        pFile->mMessageStream = NULL;
        pFile->mMemAddr = NULL;
        pFile->mMemCurrAddr = NULL;

        if (pFile->mFileLen == 0) {
            vktrace_LogError("Failed to create FileLike, file length is 0!");
            return pFile;
        }

        if (preload) {
            vktrace_LogDebug("Preload trace file to memory.");
            pFile->mMode = Memory;
            char* addr = NULL;

#if !defined(WIN32)
            // mmap does not work on Windows
            addr = (char*)mmap(NULL, pFile->mFileLen, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fileno(fp), 0);
            if (addr == MAP_FAILED) {
                vktrace_LogError("Failed to mmap tracefile for replaying.");
                addr = NULL;
            }
#else
            // TODO: Find a better solution to make it work with a large trace file
            addr = VKTRACE_NEW_ARRAY(char, pFile->mFileLen);
            if (addr == NULL) {
                vktrace_LogError("Failed to malloc memory to load tracefile for replaying.");
            } else {
                if (1 != fread(addr, (size_t)pFile->mFileLen, 1, fp)) {
                    if (ferror(fp) != 0) {
                        perror("fread error");
                    } else if (feof(fp) != 0) {
                        vktrace_LogVerbose("Reached end of file.");
                    }
                    VKTRACE_DELETE(addr);
                    addr = NULL;
                }
            }
#endif
            pFile->mMemAddr = addr;
            pFile->mMemCurrAddr = addr;
        } else {
            vktrace_LogDebug("Do not preload trace file.");
            pFile->mMode = File;
            pFile->mFile = fp;
        }
    }
    return pFile;
}

// ------------------------------------------------------------------------------------------------
FileLike* vktrace_FileLike_create_msg(MessageStream* _msgStream) {
    FileLike* pFile = NULL;
    if (_msgStream != NULL) {
        pFile = VKTRACE_NEW(FileLike);
        pFile->mMode = Socket;
        pFile->mFile = NULL;
        pFile->mMessageStream = _msgStream;
        pFile->mFileLen = 0;
        pFile->mMemAddr = NULL;
        pFile->mMemCurrAddr = NULL;
    }
    return pFile;
}

// ------------------------------------------------------------------------------------------------
void vktrace_FileLike_free(FileLike* pFileLike) {
    if (pFileLike->mMemAddr) {
#if !defined(WIN32)
        munmap(pFileLike->mMemAddr, pFileLike->mFileLen);
#else
        vktrace_free(pFileLike->mMemAddr);
#endif
    }
    vktrace_free(pFileLike);
}

// ------------------------------------------------------------------------------------------------
uint64_t vktrace_FileLike_Read(FileLike* pFileLike, void* _bytes, uint64_t _len) {
    uint64_t minSize = 0;
    uint64_t bytesInStream = 0;
    if (vktrace_FileLike_ReadRaw(pFileLike, &bytesInStream, sizeof(bytesInStream)) == FALSE) return 0;

    minSize = (_len < bytesInStream) ? _len : bytesInStream;
    if (bytesInStream > 0) {
        assert(_len >= bytesInStream);
        if (vktrace_FileLike_ReadRaw(pFileLike, _bytes, minSize) == FALSE) return 0;
    }

    return minSize;
}

// ------------------------------------------------------------------------------------------------
BOOL vktrace_FileLike_ReadRaw(FileLike* pFileLike, void* _bytes, uint64_t _len) {
    BOOL result = TRUE;
    assert((pFileLike->mFile != 0) ^ (pFileLike->mMessageStream != 0) ^ (pFileLike->mMemAddr != NULL));

    switch (pFileLike->mMode) {
        case File: {
            if (pFileLike->mFile != NULL) {
                if (1 != fread(_bytes, (size_t)_len, 1, pFileLike->mFile)) {
                    if (ferror(pFileLike->mFile) != 0) {
                        perror("fread error");
                    } else if (feof(pFileLike->mFile) != 0) {
                        vktrace_LogVerbose("Reached end of file.");
                    }
                    result = FALSE;
                }
            } else {
                result = FALSE;
            }
            break;
        }
        case Socket: {
            result = vktrace_MessageStream_BlockingRecv(pFileLike->mMessageStream, _bytes, _len);
            break;
        }
        case Memory: {
            if (pFileLike->mMemAddr == NULL) {
                result = FALSE;
            } else if (pFileLike->mMemCurrAddr + _len > pFileLike->mMemAddr + pFileLike->mFileLen) {
                vktrace_LogVerbose("Reached end of file.");
                result = FALSE;
            } else {
                memcpy(_bytes, pFileLike->mMemCurrAddr, _len);
                pFileLike->mMemCurrAddr += _len;
            }
            break;
        }

        default:
            assert(!"Invalid mode in FileLike_ReadRaw");
            result = FALSE;
    }
    return result;
}

void vktrace_FileLike_Write(FileLike* pFileLike, const void* _bytes, uint64_t _len) {
    vktrace_FileLike_WriteRaw(pFileLike, &_len, sizeof(_len));
    if (_len) {
        vktrace_FileLike_WriteRaw(pFileLike, _bytes, _len);
    }
}

// ------------------------------------------------------------------------------------------------
BOOL vktrace_FileLike_WriteRaw(FileLike* pFile, const void* _bytes, uint64_t _len) {
    BOOL result = TRUE;
    assert((pFile->mFile != 0) ^ (pFile->mMessageStream != 0));
    switch (pFile->mMode) {
        case File:
            if (1 != fwrite(_bytes, (size_t)_len, 1, pFile->mFile)) {
                result = FALSE;
            }
            break;
        case Socket:
            result = vktrace_MessageStream_Send(pFile->mMessageStream, _bytes, _len);
            break;
        default:
            assert(!"Invalid mode in FileLike_WriteRaw");
            result = FALSE;
            break;
    }
    return result;
}

// ------------------------------------------------------------------------------------------------
uint64_t vktrace_FileLike_GetCurrentPosition(FileLike* pFileLike) {
    uint64_t offset = 0;
    assert((pFileLike->mFile != 0) ^ (pFileLike->mMemAddr != NULL));

    switch (pFileLike->mMode) {
        case File: {
            offset = Ftell(pFileLike->mFile);
            break;
        }
        case Memory: {
            offset = pFileLike->mMemCurrAddr - pFileLike->mMemAddr;
            break;
        }

        default:
            assert(!"Invalid mode in vktrace_FileLike_GetCurrentPosition");
    }
    return offset;
}

// ------------------------------------------------------------------------------------------------
BOOL vktrace_FileLike_SetCurrentPosition(FileLike* pFileLike, uint64_t offset) {
    BOOL ret = FALSE;
    assert((pFileLike->mFile != 0) ^ (pFileLike->mMemAddr != NULL));

    switch (pFileLike->mMode) {
        case File: {
            if (Fseek(pFileLike->mFile, offset, SEEK_SET) == 0) {
                ret = TRUE;
            }
            break;
        }
        case Memory: {
            pFileLike->mMemCurrAddr = pFileLike->mMemAddr + offset;
            if (pFileLike->mMemCurrAddr <= (pFileLike->mMemAddr + pFileLike->mFileLen)) {
                ret = TRUE;
            }
            break;
        }

        default:
            assert(!"Invalid mode in vktrace_FileLike_SetCurrentPosition");
    }
    return ret;
}
