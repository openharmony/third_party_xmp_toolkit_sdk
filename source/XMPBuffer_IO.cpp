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

#include "public/include/XMP_Environment.h"    // ! XMP_Environment.h must be the first included header.
#include "source/XMP_LibUtils.hpp"
#include "source/XMPBuffer_IO.hpp"

#include <limits>

#define XMP_BUFFERIO_START try {
#define XMP_BUFFERIO_END1(severity)    \
    } catch ( XMP_Error & error ) {    \
        throw;                        \
    }

XMPBuffer_IO::XMPBuffer_IO(const void* buffer, XMP_Uns32 size, bool readOnly)
    : externalData(static_cast<const XMP_Uns8*>(buffer))
    , externalDataSize(size)
    , position(0)
    , readOnly(readOnly)
    , isExternalData(readOnly)
    , derivedTemp(nullptr)
{
    XMP_BUFFERIO_START
    if (buffer == nullptr || size == 0) {
        XMP_Throw("XMPBuffer_IO constructor, invalid buffer or size", kXMPErr_BadParam);
    }

    // If writable, copy the data to internal storage
    if (!readOnly) {
        data.resize(size);
        std::copy(static_cast<const XMP_Uns8*>(buffer), static_cast<const XMP_Uns8*>(buffer) + size, data.begin());
        isExternalData = false;
    }
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
}

XMPBuffer_IO::XMPBuffer_IO()
    : externalData(nullptr)
    , externalDataSize(0)
    , position(0)
    , readOnly(false)
    , isExternalData(false)
    , derivedTemp(nullptr)
{
    XMP_BUFFERIO_START
    // Initialize with empty data
    data.clear();
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
}

XMPBuffer_IO::~XMPBuffer_IO()
{
    try {
        XMP_BUFFERIO_START
        if (derivedTemp != nullptr) {
            delete derivedTemp;
            derivedTemp = nullptr;
        }
        XMP_BUFFERIO_END1(kXMPErrSev_Recoverable)
    } catch (...) {
        // All of the above is fail-safe cleanup, ignore problems.
    }
}

XMP_Uns32 XMPBuffer_IO::Read(void* buffer, XMP_Uns32 count, bool readAll)
{
    XMP_BUFFERIO_START
    if (buffer == nullptr) {
        XMP_Throw("XMPBuffer_IO::Read, null buffer", kXMPErr_BadParam);
    }

    XMP_Uns32 currentSize = GetCurrentSize();
    if (position >= static_cast<XMP_Int64>(currentSize)) {
        if (readAll) {
            XMP_Throw("XMPBuffer_IO::Read, not enough data", kXMPErr_EnforceFailure);
        }
        return 0;
    }

    XMP_Uns32 available = static_cast<XMP_Uns32>(static_cast<XMP_Int64>(currentSize) - position);
    XMP_Uns32 toRead = (count > available) ? available : count;

    if (toRead == 0) {
        if (readAll) {
            XMP_Throw("XMPBuffer_IO::Read, not enough data", kXMPErr_EnforceFailure);
        }
        return 0;
    }

    const XMP_Uns8* dataPtr = GetCurrentDataPtr();
    std::copy(dataPtr + position, dataPtr + position + toRead, static_cast<XMP_Uns8*>(buffer));
    position += toRead;
    return toRead;
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
    return 0;
}

void XMPBuffer_IO::Write(const void* buffer, XMP_Uns32 count)
{
    XMP_BUFFERIO_START
    if (readOnly) {
        XMP_Throw("XMPBuffer_IO::Write, write not permitted on read-only stream", kXMPErr_FilePermission);
    }

    if (buffer == nullptr) {
        XMP_Throw("XMPBuffer_IO::Write, null buffer", kXMPErr_BadParam);
    }

    if (position < 0) {
        XMP_Throw("XMPBuffer_IO::Write, negative position", kXMPErr_EnforceFailure);
    }
    if (count > 0 && position > (std::numeric_limits<XMP_Int64>::max() - static_cast<XMP_Int64>(count))) {
        XMP_Throw("XMPBuffer_IO::Write, size overflow", kXMPErr_ExternalFailure);
    }

    const XMP_Int64 newSize = position + static_cast<XMP_Int64>(count);
    if (newSize < 0) {
        XMP_Throw("XMPBuffer_IO::Write, invalid new size", kXMPErr_ExternalFailure);
    }
    if (static_cast<XMP_Uns64>(newSize) > static_cast<XMP_Uns64>(std::numeric_limits<size_t>::max())) {
        XMP_Throw("XMPBuffer_IO::Write, size out of range", kXMPErr_ExternalFailure);
    }

    if (static_cast<size_t>(newSize) > data.size()) {
        data.resize(static_cast<size_t>(newSize));
    }

    std::copy(static_cast<const XMP_Uns8*>(buffer), static_cast<const XMP_Uns8*>(buffer) + count,
        data.begin() + position);
    position += count;
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
}

XMP_Int64 XMPBuffer_IO::Seek(XMP_Int64 offset, SeekMode mode)
{
    XMP_BUFFERIO_START
    XMP_Int64 newPosition = position;

    switch (mode) {
        case kXMP_SeekFromStart:
            newPosition = offset;
            break;
        case kXMP_SeekFromCurrent:
            if ((offset > 0) && (position > (std::numeric_limits<XMP_Int64>::max() - offset))) {
                XMP_Throw("XMPBuffer_IO::Seek, position overflow", kXMPErr_ExternalFailure);
            }
            if ((offset < 0) && (position < (std::numeric_limits<XMP_Int64>::min() - offset))) {
                XMP_Throw("XMPBuffer_IO::Seek, position underflow", kXMPErr_ExternalFailure);
            }
            newPosition = position + offset;
            break;
        case kXMP_SeekFromEnd:
            {
                const XMP_Int64 endPos = static_cast<XMP_Int64>(GetCurrentSize());
                if ((offset > 0) && (endPos > (std::numeric_limits<XMP_Int64>::max() - offset))) {
                    XMP_Throw("XMPBuffer_IO::Seek, position overflow", kXMPErr_ExternalFailure);
                }
                if ((offset < 0) && (endPos < (std::numeric_limits<XMP_Int64>::min() - offset))) {
                    XMP_Throw("XMPBuffer_IO::Seek, position underflow", kXMPErr_ExternalFailure);
                }
                newPosition = endPos + offset;
            }
            break;
        default:
            XMP_Throw("XMPBuffer_IO::Seek, invalid seek mode", kXMPErr_BadParam);
    }

    if (newPosition < 0) {
        XMP_Throw("XMPBuffer_IO::Seek, negative position", kXMPErr_EnforceFailure);
    }

    if (readOnly && newPosition > static_cast<XMP_Int64>(GetCurrentSize())) {
        XMP_Throw("XMPBuffer_IO::Seek, read-only seek beyond EOF", kXMPErr_EnforceFailure);
    }

    position = newPosition;
    return position;
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
    return -1;
}

XMP_Int64 XMPBuffer_IO::Length()
{
    XMP_BUFFERIO_START
    return static_cast<XMP_Int64>(GetCurrentSize());
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
    return 0;
}

void XMPBuffer_IO::Truncate(XMP_Int64 length)
{
    XMP_BUFFERIO_START
    if (readOnly) {
        XMP_Throw("XMPBuffer_IO::Truncate, truncate not permitted on read-only stream", kXMPErr_FilePermission);
    }

    if (length < 0 || length > static_cast<XMP_Int64>(GetCurrentSize())) {
        XMP_Throw("XMPBuffer_IO::Truncate, invalid length", kXMPErr_EnforceFailure);
    }

    if (static_cast<XMP_Uns64>(length) > static_cast<XMP_Uns64>(std::numeric_limits<size_t>::max())) {
        XMP_Throw("XMPBuffer_IO::Truncate, size out of range", kXMPErr_ExternalFailure);
    }

    data.resize(static_cast<size_t>(length));
    if (position > length) {
        position = length;
    }
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
}

XMP_IO* XMPBuffer_IO::DeriveTemp()
{
    XMP_BUFFERIO_START
    if (readOnly) {
        XMP_Throw("XMPBuffer_IO::DeriveTemp, can't derive from read-only stream", kXMPErr_InternalFailure);
    }

    if (derivedTemp != nullptr) {
        return derivedTemp;
    }

    // Create a new temporary memory stream
    derivedTemp = new XMPBuffer_IO();
    return derivedTemp;
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
    return 0;
}

void XMPBuffer_IO::AbsorbTemp()
{
    XMP_BUFFERIO_START
    XMPBuffer_IO* temp = static_cast<XMPBuffer_IO*>(derivedTemp);
    if (temp == nullptr) {
        XMP_Throw("XMPBuffer_IO::AbsorbTemp, no temp to absorb", kXMPErr_InternalFailure);
    }

    // Replace current data with temp data
    data = temp->data;
    position = temp->position;

    // Clean up temp
    delete temp;
    derivedTemp = nullptr;
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
}

void XMPBuffer_IO::DeleteTemp()
{
    XMP_BUFFERIO_START
    if (derivedTemp != nullptr) {
        delete derivedTemp;
        derivedTemp = nullptr;
    }
    XMP_BUFFERIO_END1(kXMPErrSev_FileFatal)
}

const void* XMPBuffer_IO::GetDataPtr() const
{
    return GetCurrentDataPtr();
}

const XMP_Uns8* XMPBuffer_IO::GetCurrentDataPtr() const
{
    return isExternalData ? externalData : data.data();
}

XMP_Uns32 XMPBuffer_IO::GetCurrentSize() const
{
     return isExternalData ? externalDataSize : static_cast<XMP_Uns32>(data.size());
}
