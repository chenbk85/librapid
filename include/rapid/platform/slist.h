//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>

#include <rapid/platform/platform.h>

namespace rapid {

namespace platform {

template <typename T, typename Allocator = std::allocator<T>>
class SList {
public:
    typedef typename Allocator::value_type value_type;
    typedef typename Allocator::pointer pointer;
    typedef typename Allocator::reference reference;

    SList();

    ~SList();

    SList(SList const &) = delete;
    SList& operator=(SList const &) = delete;

    bool tryDequeue(reference element);

    template <typename... Args>
    void enqueue(Args&&... args);

    bool isEmpty() const;

    uint16_t count() const;

    void clear();

private:
    struct SListEntry {
#define PAD_SIZE \
	CACHE_LINE_PAD_SIZE - (sizeof(SLIST_ENTRY) + sizeof(value_type)) % CACHE_LINE_PAD_SIZE
        SLIST_ENTRY itemEntry;
		value_type element;
		char pad[PAD_SIZE];
    };

    static PSLIST_HEADER allocateListHead();

    SListEntry * allcateSListEntry();

    void destorySListEntry(SListEntry *pEntry, reference element);

    PSLIST_HEADER pListHead_;
    Allocator allocator_;
};

template <typename T, typename Allocator>
SList<T, Allocator>::SList()
    : pListHead_(allocateListHead()) {
}

template <typename T, typename Allocator>
SList<T, Allocator>::~SList() {
    clear();
    ::InterlockedFlushSList(pListHead_);
    if (pListHead_ != nullptr) {
        ::_mm_free(pListHead_);
    }
}

template <typename T, typename Allocator>
template <typename... Args>
void SList<T, Allocator>::enqueue(Args&&... args) {
    auto pItem = allcateSListEntry();
	allocator_.construct(allocator_.address(pItem->element), std::forward<Args>(args)...);
    ::InterlockedPushEntrySList(pListHead_, &pItem->itemEntry);
}

template <typename T, typename Allocator>
bool SList<T, Allocator>::tryDequeue(reference element) {
	auto pItem = reinterpret_cast<SListEntry*>(::InterlockedPopEntrySList(pListHead_));
    if (pItem != nullptr) {
        destorySListEntry(pItem, element);
        return true;
    }
    return false;
}

template <typename T, typename Allocator>
bool SList<T, Allocator>::isEmpty() const {
    return count() == 0;
}

template <typename T, typename Allocator>
uint16_t SList<T, Allocator>::count() const {
    return ::QueryDepthSList(pListHead_);
}

template <typename T, typename Allocator>
typename SList<T, Allocator>::SListEntry * SList<T, Allocator>::allcateSListEntry() {
	// tcmalloc可以解決插入一個內容需要_mm_malloc效率問題!
    return reinterpret_cast<SListEntry*>(::_mm_malloc(sizeof(SListEntry), 
		MEMORY_ALLOCATION_ALIGNMENT));
}

template <typename T, typename Allocator>
PSLIST_HEADER SList<T, Allocator>::allocateListHead() {
    auto head = reinterpret_cast<PSLIST_HEADER>(::_mm_malloc(sizeof(SLIST_HEADER),
                MEMORY_ALLOCATION_ALIGNMENT));
    ::InitializeSListHead(head);
    return head;
}

template <typename T, typename Allocator>
void SList<T, Allocator>::destorySListEntry(SListEntry *pItem, reference element) {
	element = std::move(*allocator_.address(pItem->element));
	allocator_.destroy(allocator_.address(pItem->element));
    ::_mm_free(pItem);
}

template <typename T, typename Allocator>
void SList<T, Allocator>::clear() {
    for (auto *pItem = reinterpret_cast<SListEntry*>(::InterlockedPopEntrySList(pListHead_));
            pItem != nullptr;
            pItem = reinterpret_cast<SListEntry*>(::InterlockedPopEntrySList(pListHead_))) {
		allocator_.destroy(allocator_.address(pItem->element));
        ::_mm_free(pItem);
    }
}

}

}
