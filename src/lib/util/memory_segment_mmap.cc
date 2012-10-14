// Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include "memory_segment_mmap.h"
#include <exceptions/exceptions.h>

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/offset_ptr.hpp>

#include <algorithm>
#include <new>                  // for nothrow

using namespace boost::interprocess;

namespace isc {
namespace util {

MemorySegmentMmap::MemorySegmentMmap(const std::string& filename,
                                     bool create, size_t initial_size) :
        filename_(filename),
        base_sgmt_(NULL),
        allocated_size_(0)
{
    if (create) {
        base_sgmt_ = new managed_mapped_file(open_or_create, filename_.c_str(),
                                             initial_size);
    } else {
        base_sgmt_ = new managed_mapped_file(open_only, filename_.c_str());
    }
}

MemorySegmentMmap::~MemorySegmentMmap() {
    delete base_sgmt_;
}

void*
MemorySegmentMmap::allocate(size_t size) {
    void* ptr = NULL;

    // managed_mapped_file doesn't seem to reject too big size correctly, so
    // we check get_free_memory by hand first.
    if (base_sgmt_->get_free_memory() >= size) { 
        ptr = base_sgmt_->allocate(size, std::nothrow);
    }
    if (ptr == NULL) {
        // Try growing the size once
        const size_t prev_size = base_sgmt_->get_size();
        delete base_sgmt_;
        base_sgmt_ = NULL;
        // Increase the size so that the new size will be double the original
        // size until it reaches 256MB (arbitrary choice).  After that we
        // increase the size 256MB each time.
        const size_t new_size = std::min(prev_size * 2,
                                         prev_size + 1024 * 1024 * 256);
        if (!managed_mapped_file::grow(filename_.c_str(),
                                       new_size - prev_size)) {
            throw std::bad_alloc();
        }
        base_sgmt_ = new managed_mapped_file(open_only, filename_.c_str());
        isc_throw(SegmentGrown, "mmap memory segment grown, size: " 
                  << base_sgmt_->get_size() << ", free: "
                  << base_sgmt_->get_free_memory());
    }

    allocated_size_ += size;
    return (ptr);
}

void
MemorySegmentMmap::deallocate(void* ptr, size_t size) {
    if (ptr == NULL) {
        // Return early if NULL is passed to be deallocated (without
        // modifying allocated_size, or comparing against it).
        return;
    }

    if (size > allocated_size_) {
      isc_throw(OutOfRange, "Invalid size to deallocate: " << size
                << "; currently allocated size: " << allocated_size_);
    }

    allocated_size_ -= size;
    base_sgmt_->deallocate(ptr);
}

bool
MemorySegmentMmap::allMemoryDeallocated() const {
    return (base_sgmt_->all_memory_deallocated());
}

void
MemorySegmentMmap::setNamedAddress(const char* name, void* addr) {
    base_sgmt_->find_or_construct<offset_ptr<void> >(name)(addr);
}

void*
MemorySegmentMmap::getNamedAddress(const char* name) {
    offset_ptr<void>* storage =
        base_sgmt_->find<offset_ptr<void> >(name).first;
    if (storage != NULL) {
        return (storage->get());
    } else {
        return (NULL);
    }
}

void
MemorySegmentMmap::clearNamedAddress(const char* name) {
    base_sgmt_->destroy<offset_ptr<void> >(name);
}

} // namespace util
} // namespace isc
