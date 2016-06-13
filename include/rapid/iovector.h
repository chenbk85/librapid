//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <vector>

#include <Winsock2.h>

namespace rapid {

class IoVector {
public:
    IoVector() = default;

    ~IoVector() = default;

    void append(char * buffer, ULONG bufferLen);

    void add(char * buffer, ULONG bufferLen);

    ULONG removeAndCopy(char* buffer, ULONG bufferLen);

    size_t remove(ULONG bufferLen);

    void resize(ULONG count);

    WSABUF * iovec();

    size_t count() const;

    bool isEmpty() const;

    void clear();

    LONG size() const;

private:
    std::vector<WSABUF> iovtor_;
};

inline void IoVector::append(char * buffer, ULONG bufferLen) {
    if (buffer != nullptr && bufferLen > 0) {
        if (iovtor_.size() > 0) {
            auto & last = iovtor_.back();
            // 判斷是否為連續的記憶體位址將len往後累加.
            if (static_cast<char*>(last.buf) + last.len == buffer) {
                last.len += bufferLen;
                return;
            }
        }
    }
    add(buffer, bufferLen);
}

inline void IoVector::add(char * buffer, ULONG bufferLen) {
    WSABUF tmp = { bufferLen, buffer };
    iovtor_.push_back(tmp);
}

inline size_t IoVector::remove(ULONG bufferLen) {
    auto itr = iovtor_.begin();
    auto end = iovtor_.end();

    uint32_t bytesToRemove = bufferLen;

    for (; itr < end && bytesToRemove >= itr->len; ++itr) {
        bytesToRemove -= itr->len;
    }

    iovtor_.erase(iovtor_.begin(), itr);

    if (!iovtor_.empty() && bytesToRemove != 0) {
        iovtor_[0].buf = static_cast<char*>(iovtor_[0].buf) + bytesToRemove;
        iovtor_[0].len -= bytesToRemove;
        return bufferLen;
    }
    return bufferLen - bytesToRemove;
}

inline ULONG IoVector::removeAndCopy(char* buffer, ULONG bufferLen) {
    auto itr = iovtor_.begin();
    auto end = iovtor_.end();

    ULONG bytesToRemove = bufferLen;

    for (; itr < end && bytesToRemove >= itr->len; ++itr) {
        memcpy(buffer, itr->buf, itr->len);
        bytesToRemove -= itr->len;
        buffer += itr->len;
    }

    if (bytesToRemove == 0) {
        return bytesToRemove;
    }

    if (iovtor_.empty()) {
        return bufferLen - bytesToRemove;
    }

    iovtor_.erase(iovtor_.begin(), itr);

    memcpy(buffer, iovtor_[0].buf, bytesToRemove);
    iovtor_[0].buf = static_cast<char*>(iovtor_[0].buf) + bytesToRemove;
    iovtor_[0].len -= bytesToRemove;

    return bufferLen;
}

inline WSABUF * IoVector::iovec() {
    return iovtor_.data();
}

inline size_t IoVector::count() const {
    return iovtor_.size();
}

inline void IoVector::resize(ULONG count) {
    iovtor_.resize(count);
}

inline bool IoVector::isEmpty() const {
    return iovtor_.empty();
}

inline void IoVector::clear() {
    iovtor_.clear();
}

inline LONG IoVector::size() const {
    LONG totalSize = 0;

    for (auto itr : iovtor_) {
        totalSize += itr.len;
    }
    return totalSize;
}

}
