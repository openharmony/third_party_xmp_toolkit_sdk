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

#ifndef __XMPBuffer_IO_hpp__
#define __XMPBuffer_IO_hpp__ 1

#include "public/include/XMP_Environment.h"    // ! XMP_Environment.h must be the first included header.
#include "public/include/XMP_Const.h"
#include "public/include/XMP_IO.hpp"

#include <string>
#include <vector>

// =================================================================================================

class XMPBuffer_IO : public XMP_IO {
    // Implementation class for I/O using memory buffer data, allows XMP-Toolkit-SDK to work with
    // pre-loaded file data instead of file paths. This is useful when you have already read
    // file data into memory and want to parse XMP metadata from it.
public:
    // ---------------------------------------------------------------------------------------------
    /// @brief Constructor for read-only memory stream
    ///
    /// Creates a read-only XMPBuffer_IO object from existing memory data.
    ///
    /// @param buffer Pointer to the memory buffer containing file data
    /// @param size Size of the memory buffer in bytes

    XMPBuffer_IO(const void* buffer, XMP_Uns32 size, bool readOnly);

    // ---------------------------------------------------------------------------------------------
    /// @brief Constructor for writable memory stream
    ///
    /// Creates a writable XMPBuffer_IO object that can be used for writing XMP data.

    XMPBuffer_IO();

    virtual ~XMPBuffer_IO();

    // XMP_IO interface implementation
    virtual XMP_Uns32 Read(void* buffer, XMP_Uns32 count, bool readAll = false);
    virtual void Write(const void* buffer, XMP_Uns32 count);
    virtual XMP_Int64 Seek(XMP_Int64 offset, SeekMode mode);
    virtual XMP_Int64 Length();
    virtual void Truncate(XMP_Int64 length);
    virtual XMP_IO* DeriveTemp();
    virtual void AbsorbTemp();
    virtual void DeleteTemp();

    // ---------------------------------------------------------------------------------------------
    /// @brief Get pointer to the data buffer
    ///
    /// Returns a pointer to the internal data buffer.
    ///
    /// @return Pointer to the data buffer

    const void* GetDataPtr() const;

    // ---------------------------------------------------------------------------------------------
    /// @brief Get the size of the data
    ///
    /// Returns the current size of the data in the stream.
    ///
    /// @return Size of the data in bytes

    XMP_Uns32 GetDataSize() const { return GetCurrentSize(); }

private:
    std::vector<XMP_Uns8> data;     // Internal data storage
    const XMP_Uns8* externalData;   // Zero-copy mode external data pointer
    XMP_Uns32 externalDataSize;     // Data size
    XMP_Int64 position;             // Current read/write position
    bool readOnly;                  // Whether this stream is read-only
    bool isExternalData;            // Flag to indicate whether external data is used (zero-copy)
    XMP_IO* derivedTemp;            // Associated temporary stream

    const XMP_Uns8* GetCurrentDataPtr() const;     // Get current data pointer
    XMP_Uns32 GetCurrentSize() const;              // Get current data size

    // The copy constructor and assignment operators are private to prevent client use. Allowing
    // them would require shared I/O state between XMPBuffer_IO objects.
    XMPBuffer_IO(const XMPBuffer_IO& original);
    void operator=(const XMP_IO& in);
    void operator=(const XMPBuffer_IO& in);
};

// =================================================================================================

#endif    // __XMPBuffer_IO_hpp__
