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

#include "source/XMP_LibUtils.hpp"
#include "XMPFd_IO.hpp"

#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <limits>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr size_t kErrMsgMax = 256;
constexpr size_t kCopyBufferSize = 8192;
constexpr size_t kFdLinkPathMax = 64;
constexpr int kMaxTempFileSuffix = 100;
constexpr int kTempSuffixDigits = 2;
constexpr mode_t kDefaultTempFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
constexpr size_t kDerivedTempSuffixBufferSize = 16;

bool TryGetFdPath(int fd, std::string &filePath)
{
    char fdLinkPath[kFdLinkPathMax] = {0};
    int linkPathLen = snprintf(fdLinkPath, sizeof(fdLinkPath), "/proc/self/fd/%d", fd);
    if (linkPathLen <= 0 || static_cast<size_t>(linkPathLen) >= sizeof(fdLinkPath)) {
        return false;
    }

    char resolvedPath[PATH_MAX] = {0};
    ssize_t resolvedPathLen = readlink(fdLinkPath, resolvedPath, sizeof(resolvedPath) - 1);
    if (resolvedPathLen <= 0) {
        return false;
    }

    resolvedPath[resolvedPathLen] = '\0';
    filePath.assign(resolvedPath, static_cast<size_t>(resolvedPathLen));
    return !filePath.empty() && filePath[0] == '/';
}

std::string CreateDerivedTempPath(const std::string &sourcePath, int index)
{
    if (index < 0 || index >= kMaxTempFileSuffix) {
        return "";
    }

    char suffix[kDerivedTempSuffixBufferSize] = {0};
    int suffixLen = snprintf(suffix, sizeof(suffix), "._%0*d_", kTempSuffixDigits, index);
    if (suffixLen <= 0 || static_cast<size_t>(suffixLen) >= sizeof(suffix)) {
        return "";
    }

    std::string tempPath = sourcePath;
    tempPath += suffix;
    return tempPath;
}

bool TryCreateDerivedTempFd(const std::string &sourcePath, int &tempFd, std::string &tempPath, int &err)
{
    for (int index = 0; index < kMaxTempFileSuffix; ++index) {
        tempPath = CreateDerivedTempPath(sourcePath, index);
        if (tempPath.empty()) {
            err = EINVAL;
            return false;
        }
        tempFd = open(tempPath.c_str(), O_CREAT | O_EXCL | O_RDWR, kDefaultTempFileMode);
        if (tempFd >= 0) {
            return true;
        }
        if (errno == EEXIST) {
            continue;
        }
        err = errno;
        return false;
    }

    err = EEXIST;
    return false;
}
} // anonymous namespace

#define XMP_FDIO_START try {
#define XMP_FDIO_END1(severity)        \
    } catch ( XMP_Error & error ) {    \
        throw;                        \
    }

XMPFd_IO::XMPFd_IO(int fd, bool readOnly, bool takeOwnership)
    : fd_(fd), readOnly_(readOnly), ownsFd_(takeOwnership), derivedTemp_(nullptr)
{
    XMP_FDIO_START
    this->ValidateFdOrThrow("XMPFd_IO::XMPFd_IO");
    this->ValidateAccessModeOrThrow("XMPFd_IO::XMPFd_IO");
    this->ValidateSeekableOrThrow("XMPFd_IO::XMPFd_IO");
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
}

XMPFd_IO::XMPFd_IO(const std::string& filePath, bool readOnly)
    : fd_(-1), readOnly_(readOnly), ownsFd_(true), derivedTemp_(nullptr)
{
    XMP_FDIO_START
    int flags = readOnly_ ? O_RDONLY : O_RDWR;
    fd_ = open(filePath.c_str(), flags);
    if (fd_ < 0) {
        this->ThrowErrnoExternalFailure("XMPFd_IO::XMPFd_IO(open)", errno);
    }
    this->ValidateFdOrThrow("XMPFd_IO::XMPFd_IO(open)");
    this->ValidateAccessModeOrThrow("XMPFd_IO::XMPFd_IO(open)");
    this->ValidateSeekableOrThrow("XMPFd_IO::XMPFd_IO(open)");
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
}

XMPFd_IO::~XMPFd_IO()
{
    try {
        if (derivedTemp_ != nullptr) {
            delete derivedTemp_;
            derivedTemp_ = nullptr;
        }

        if (ownsFd_ && fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }

        if (!tempFilePath_.empty()) {
            unlink(tempFilePath_.c_str());
            tempFilePath_.clear();
        }
    } catch (...) {
    }
}

XMP_Uns32 XMPFd_IO::Read(void* buffer, XMP_Uns32 count, bool readAll)
{
    XMP_FDIO_START
    this->ValidateFdOrThrow("XMPFd_IO::Read");
    this->ValidateAccessModeOrThrow("XMPFd_IO::Read");
    this->ValidateSeekableOrThrow("XMPFd_IO::Read");
    if (buffer == nullptr) {
        XMP_Throw("XMPFd_IO::Read, null buffer", kXMPErr_BadParam);
    }

    XMP_Uns32 total = 0;
    while (total < count) {
        const XMP_Uns32 remaining = count - total;
        ssize_t bytesRead = read(fd_, static_cast<char*>(buffer) + total, remaining);
        if (bytesRead > 0) {
            total += static_cast<XMP_Uns32>(bytesRead);
            if (!readAll) {
                break;
            }
            continue;
        }
        if (bytesRead == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        this->ThrowErrnoExternalFailure("XMPFd_IO::Read(read)", errno);
    }

    if (readAll && total < count) {
        XMP_Throw("XMPFd_IO::Read, not enough data", kXMPErr_EnforceFailure);
    }

    return total;
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
    return 0;
}

void XMPFd_IO::Write(const void* buffer, XMP_Uns32 count)
{
    XMP_FDIO_START
    this->ValidateWritableOrThrow("XMPFd_IO::Write");
    this->ValidateSeekableOrThrow("XMPFd_IO::Write");
    if (buffer == nullptr) {
        XMP_Throw("XMPFd_IO::Write, null buffer", kXMPErr_BadParam);
    }

    XMP_Uns32 total = 0;
    while (total < count) {
        const XMP_Uns32 remaining = count - total;
        ssize_t bytesWritten = write(fd_, static_cast<const char*>(buffer) + total, remaining);
        if (bytesWritten > 0) {
            total += static_cast<XMP_Uns32>(bytesWritten);
            continue;
        }
        if (bytesWritten == 0) {
            XMP_Throw("XMPFd_IO::Write, write returned 0", kXMPErr_ExternalFailure);
        }
        if (errno == EINTR) {
            continue;
        }
        this->ThrowErrnoExternalFailure("XMPFd_IO::Write(write)", errno);
    }
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
}

XMP_Int64 XMPFd_IO::Seek(XMP_Int64 offset, SeekMode mode)
{
    XMP_FDIO_START
    this->ValidateFdOrThrow("XMPFd_IO::Seek");
    this->ValidateAccessModeOrThrow("XMPFd_IO::Seek");
    this->ValidateSeekableOrThrow("XMPFd_IO::Seek");

    int whence;
    switch (mode) {
        case kXMP_SeekFromStart:
            whence = SEEK_SET;
            break;
        case kXMP_SeekFromCurrent:
            whence = SEEK_CUR;
            break;
        case kXMP_SeekFromEnd:
            whence = SEEK_END;
            break;
        default:
            XMP_Throw("XMPFd_IO::Seek, invalid seek mode", kXMPErr_BadParam);
    }

    if (!this->CanConvertToOffT(offset)) {
        XMP_Throw("XMPFd_IO::Seek, offset out of range", kXMPErr_ExternalFailure);
    }

    off_t newPosition = lseek(fd_, static_cast<off_t>(offset), whence);
    if (newPosition < 0) {
        this->ThrowErrnoExternalFailure("XMPFd_IO::Seek(lseek)", errno);
    }

    return static_cast<XMP_Int64>(newPosition);
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
    return -1;
}

XMP_Int64 XMPFd_IO::Length()
{
    XMP_FDIO_START
    this->ValidateFdOrThrow("XMPFd_IO::Length");
    this->ValidateAccessModeOrThrow("XMPFd_IO::Length");

    struct stat fileStat;
    if (fstat(fd_, &fileStat) < 0) {
        this->ThrowErrnoExternalFailure("XMPFd_IO::Length(fstat)", errno);
    }

    return static_cast<XMP_Int64>(fileStat.st_size);
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
    return -1;
}

void XMPFd_IO::Truncate(XMP_Int64 length)
{
    XMP_FDIO_START
    this->ValidateWritableOrThrow("XMPFd_IO::Truncate");
    this->ValidateSeekableOrThrow("XMPFd_IO::Truncate");
    if (length < 0) {
        XMP_Throw("XMPFd_IO::Truncate, invalid length", kXMPErr_BadParam);
    }
    if (!this->CanConvertToOffT(length)) {
        XMP_Throw("XMPFd_IO::Truncate, length out of range", kXMPErr_ExternalFailure);
    }

    if (ftruncate(fd_, static_cast<off_t>(length)) < 0) {
        this->ThrowErrnoExternalFailure("XMPFd_IO::Truncate(ftruncate)", errno);
    }

    // Adjust position if beyond new length
    off_t currentPos = lseek(fd_, 0, SEEK_CUR);
    if (currentPos > static_cast<off_t>(length)) {
        lseek(fd_, static_cast<off_t>(length), SEEK_SET);
    }
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
}

XMP_IO* XMPFd_IO::DeriveTemp()
{
    XMP_FDIO_START
    this->ValidateWritableOrThrow("XMPFd_IO::DeriveTemp");
    this->ValidateSeekableOrThrow("XMPFd_IO::DeriveTemp");

    if (derivedTemp_ != nullptr) {
        return derivedTemp_;
    }

    std::string sourcePath;
    if (!TryGetFdPath(fd_, sourcePath)) {
        XMP_Throw("XMPFd_IO::DeriveTemp, failed to resolve source path", kXMPErr_ExternalFailure);
    }

    int tempFd = -1;
    int tempErrno = 0;
    std::string tempPath;
    if (!TryCreateDerivedTempFd(sourcePath, tempFd, tempPath, tempErrno)) {
        this->ThrowErrnoExternalFailure("XMPFd_IO::DeriveTemp(open)", tempErrno);
    }

    try {
        derivedTemp_ = new XMPFd_IO(tempFd, false, true);
    } catch (...) {
        close(tempFd);
        unlink(tempPath.c_str());
        throw;
    }
    tempFilePath_ = tempPath;
    return derivedTemp_;
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
    return nullptr;
}

void XMPFd_IO::AbsorbTemp()
{
    XMP_FDIO_START
    XMPFd_IO* temp = static_cast<XMPFd_IO*>(derivedTemp_);
    if (temp == nullptr) {
        XMP_Throw("XMPFd_IO::AbsorbTemp, no temp to absorb", kXMPErr_InternalFailure);
    }
    this->ValidateWritableOrThrow("XMPFd_IO::AbsorbTemp");
    this->ValidateSeekableOrThrow("XMPFd_IO::AbsorbTemp");

    int tempFd = temp->GetFd();
    if (tempFd < 0) {
        XMP_Throw("XMPFd_IO::AbsorbTemp, invalid temp file descriptor", kXMPErr_BadParam);
    }

    SeekToStartOrThrow(fd_, "XMPFd_IO::AbsorbTemp(lseek orig)");
    SeekToStartOrThrow(tempFd, "XMPFd_IO::AbsorbTemp(lseek temp)");
    TruncateToZeroOrThrow(fd_, "XMPFd_IO::AbsorbTemp(ftruncate)");
    CopyAllOrThrow(tempFd, fd_, "XMPFd_IO::AbsorbTemp(read)", "XMPFd_IO::AbsorbTemp(write)");

    // Cleanup temp file
    delete temp;
    derivedTemp_ = nullptr;

    if (!tempFilePath_.empty()) {
        unlink(tempFilePath_.c_str());
        tempFilePath_.clear();
    }

    SeekToStartOrThrow(fd_, "XMPFd_IO::AbsorbTemp(lseek reset)");
    XMP_FDIO_END1(kXMPErrSev_FileFatal)
}

void XMPFd_IO::DeleteTemp()
{
    if (derivedTemp_ != nullptr) {
        delete derivedTemp_;
        derivedTemp_ = nullptr;
    }

    if (!tempFilePath_.empty()) {
        unlink(tempFilePath_.c_str());
        tempFilePath_.clear();
    }
}

int XMPFd_IO::GetFd() const
{
    return fd_;
}

bool XMPFd_IO::IsValid() const
{
    // Best-effort validation for external callers; detailed failures are reported by Validate*OrThrow.
    if (fd_ < 0) {
        return false;
    }
    if (fcntl(fd_, F_GETFD) == -1) {
        return false;
    }
    if (lseek(fd_, 0, SEEK_CUR) == static_cast<off_t>(-1)) {
        if (errno == ESPIPE) {
            return false;
        }
    }
    return true;
}

void XMPFd_IO::ValidateFdOrThrow(const char* context) const
{
    if (fd_ < 0) {
        XMP_Throw("XMPFd_IO, invalid file descriptor", kXMPErr_BadParam);
    }
    if (fcntl(fd_, F_GETFD) == -1) {
        ThrowErrnoExternalFailure(context, errno);
    }
}

void XMPFd_IO::ValidateWritableOrThrow(const char* context) const
{
    this->ValidateFdOrThrow(context);
    if (readOnly_) {
        XMP_Throw("XMPFd_IO, write not permitted on read-only stream", kXMPErr_FilePermission);
    }
    this->ValidateAccessModeOrThrow(context);
}

void XMPFd_IO::ValidateSeekableOrThrow(const char* context) const
{
    this->ValidateFdOrThrow(context);
    if (lseek(fd_, 0, SEEK_CUR) == static_cast<off_t>(-1)) {
        ThrowErrnoExternalFailure(context, errno);
    }
}

void XMPFd_IO::ValidateAccessModeOrThrow(const char* context) const
{
    this->ValidateFdOrThrow(context);
    const int flags = fcntl(fd_, F_GETFL);
    if (flags == -1) {
        ThrowErrnoExternalFailure(context, errno);
    }

    const int accMode = (flags & O_ACCMODE);
    if (readOnly_) {
        if (accMode == O_WRONLY) {
            XMP_Throw("XMPFd_IO, fd is write-only but stream is read-only", kXMPErr_ExternalFailure);
        }
    } else {
        if (accMode == O_RDONLY) {
            XMP_Throw("XMPFd_IO, fd is read-only but stream is writable", kXMPErr_ExternalFailure);
        }
    }
}

void XMPFd_IO::ThrowErrnoExternalFailure(const char* context, int err)
{
    char msg[kErrMsgMax];
    const char* errStr = strerror(err);
    if (errStr == nullptr) {
        errStr = "unknown";
    }
    snprintf(msg, sizeof(msg), "%s, errno=%d(%s)", context, err, errStr);
    XMP_Throw(msg, kXMPErr_ExternalFailure);
}

void XMPFd_IO::SeekToStartOrThrow(int fd, const char* context)
{
    if (lseek(fd, 0, SEEK_SET) < 0) {
        ThrowErrnoExternalFailure(context, errno);
    }
}

void XMPFd_IO::TruncateToZeroOrThrow(int fd, const char* context)
{
    if (ftruncate(fd, 0) < 0) {
        ThrowErrnoExternalFailure(context, errno);
    }
}

void XMPFd_IO::CopyAllOrThrow(int srcFd, int dstFd, const char* contextRead, const char* contextWrite)
{
    char buffer[kCopyBufferSize];
    while (true) {
        ssize_t bytesRead = read(srcFd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            ssize_t writtenTotal = 0;
            while (writtenTotal < bytesRead) {
                ssize_t bytesWritten = write(dstFd, buffer + writtenTotal, bytesRead - writtenTotal);
                if (bytesWritten > 0) {
                    writtenTotal += bytesWritten;
                    continue;
                }
                if (bytesWritten == 0) {
                    XMP_Throw("XMPFd_IO::CopyAllOrThrow, write returned 0", kXMPErr_ExternalFailure);
                }
                if (errno == EINTR) {
                    continue;
                }
                ThrowErrnoExternalFailure(contextWrite, errno);
            }
            continue;
        }
        if (bytesRead == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        ThrowErrnoExternalFailure(contextRead, errno);
    }
}

bool XMPFd_IO::CanConvertToOffT(XMP_Int64 value)
{
    if (value < static_cast<XMP_Int64>(std::numeric_limits<off_t>::min())) {
        return false;
    }
    if (value > static_cast<XMP_Int64>(std::numeric_limits<off_t>::max())) {
        return false;
    }
    return true;
}
