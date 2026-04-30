/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
 */

#ifndef __XMPFd_IO_hpp__
#define __XMPFd_IO_hpp__ 1

#include "public/include/XMP_Environment.h"
#include "public/include/XMP_Const.h"
#include "public/include/XMP_IO.hpp"

#include <string>

// =================================================================================================
/// @brief XMPFd_IO - File descriptor I/O implementation for XMP SDK
///
/// This class implements the XMP_IO interface to allow XMP SDK to work with
/// file descriptors instead of file paths. This enables reading and writing
/// XMP metadata using file descriptors for more flexible file operations.
// =================================================================================================

class XMPFd_IO : public XMP_IO {
public:
    XMPFd_IO(int fd, bool readOnly, bool takeOwnership = false);
    XMPFd_IO(const std::string& filePath, bool readOnly);

    virtual ~XMPFd_IO();

    virtual XMP_Uns32 Read(void* buffer, XMP_Uns32 count, bool readAll = false) override;
    virtual void Write(const void* buffer, XMP_Uns32 count) override;
    virtual XMP_Int64 Seek(XMP_Int64 offset, SeekMode mode) override;
    virtual XMP_Int64 Length() override;
    virtual void Truncate(XMP_Int64 length) override;
    virtual XMP_IO* DeriveTemp() override;
    virtual void AbsorbTemp() override;
    virtual void DeleteTemp() override;

    int GetFd() const;
    bool IsValid() const;

private:
    void ValidateFdOrThrow(const char* context) const;
    void ValidateWritableOrThrow(const char* context) const;
    void ValidateSeekableOrThrow(const char* context) const;
    void ValidateAccessModeOrThrow(const char* context) const;
    static void SeekToStartOrThrow(int fd, const char* context);
    static void TruncateToZeroOrThrow(int fd, const char* context);
    static void CopyAllOrThrow(int srcFd, int dstFd, const char* contextRead, const char* contextWrite);
    static void ThrowErrnoExternalFailure(const char* context, int err);
    static bool CanConvertToOffT(XMP_Int64 value);

    int fd_;
    bool readOnly_;
    bool ownsFd_;
    XMP_IO* derivedTemp_;
    std::string tempFilePath_;

    // The copy constructor and assignment operators are private to prevent client use. Allowing
    // them would require shared I/O state between XMPFiles_IO objects.
    XMPFd_IO(const XMPFd_IO& original);
    void operator=(const XMP_IO& in);
    void operator=(const XMPFd_IO& in);
};

#endif // __XMPFd_IO_hpp__