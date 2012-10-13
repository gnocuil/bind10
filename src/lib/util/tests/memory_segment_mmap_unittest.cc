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

#include "util/memory_segment.h"
#include "util/memory_segment_mmap.h"
#include <exceptions/exceptions.h>
#include <gtest/gtest.h>

#include <boost/scoped_ptr.hpp>

#include <limits.h>
#include <unistd.h>

using namespace std;
using boost::scoped_ptr;
using namespace isc::util;

namespace {

const char* const TEST_MMAP_FILE = "test.mapped";

class MemorySegmentMmapTest : public ::testing::Test {
protected:
    MemorySegmentMmapTest() {
        unlink(TEST_MMAP_FILE);
        mmap_segment.reset(new MemorySegmentMmap(TEST_MMAP_FILE, true));
        segment = mmap_segment.get();
    }

    scoped_ptr<MemorySegmentMmap> mmap_segment;
    MemorySegment* segment;
};

TEST_F(MemorySegmentMmapTest, Local) {
    // By default, nothing is allocated.
    EXPECT_TRUE(segment->allMemoryDeallocated());

    void* ptr = segment->allocate(1024);
    mmap_segment->setNamedAddress("testptr", ptr);
    EXPECT_EQ(ptr, mmap_segment->getNamedAddress("testptr"));
    mmap_segment->clearNamedAddress("testptr");

    // Now, we have an allocation:
    EXPECT_FALSE(segment->allMemoryDeallocated());

    void* ptr2 = segment->allocate(42);

    // Still:
    EXPECT_FALSE(segment->allMemoryDeallocated());

    // These should not fail, because the buffers have been allocated.
    EXPECT_NO_FATAL_FAILURE(memset(ptr, 0, 1024));
    EXPECT_NO_FATAL_FAILURE(memset(ptr, 0, 42));

    segment->deallocate(ptr, 1024);

    // Still:
    EXPECT_FALSE(segment->allMemoryDeallocated());

    segment->deallocate(ptr2, 42);

    // Now, we have an deallocated everything:
    EXPECT_TRUE(segment->allMemoryDeallocated());
}

TEST_F(MemorySegmentMmapTest, OverMemory) {
    size_t retried = 0;
    vector<void*> pointers;
    for (size_t i = 0; i < 32; ++i) {
        try {
            pointers.push_back(segment->allocate(1024));
        } catch (const MemorySegment::SegmentGrown&) {
            // retry, this should succeed.
            pointers.push_back(segment->allocate(1024));
            ++retried;
        }
    }
    EXPECT_EQ(1, retried);
}

TEST_F(MemorySegmentMmapTest, TooMuchMemory) {
    EXPECT_THROW(segment->allocate(ULONG_MAX), MemorySegment::SegmentGrown);
    // retry wouldn't succeed.
    EXPECT_THROW(segment->allocate(ULONG_MAX), MemorySegment::SegmentGrown);
}

TEST_F(MemorySegmentMmapTest, BadDeallocate) {
    // By default, nothing is allocated.
    EXPECT_TRUE(segment->allMemoryDeallocated());

    void* ptr = segment->allocate(1024);

    // Now, we have an allocation:
    EXPECT_FALSE(segment->allMemoryDeallocated());

    // This should not throw
    EXPECT_NO_THROW(segment->deallocate(ptr, 1024));

    // Now, we have an deallocated everything:
    EXPECT_TRUE(segment->allMemoryDeallocated());

    ptr = segment->allocate(1024);

    // Now, we have another allocation:
    EXPECT_FALSE(segment->allMemoryDeallocated());

    // This should throw as the size passed to deallocate() is larger
    // than what was allocated.
    EXPECT_THROW(segment->deallocate(ptr, 2048), isc::OutOfRange);

    // This should not throw
    EXPECT_NO_THROW(segment->deallocate(ptr, 1024));

    // Now, we have an deallocated everything:
    EXPECT_TRUE(segment->allMemoryDeallocated());
}

TEST_F(MemorySegmentMmapTest, NullDeallocate) {
    // By default, nothing is allocated.
    EXPECT_TRUE(segment->allMemoryDeallocated());

    // NULL deallocation is a no-op.
    EXPECT_NO_THROW(segment->deallocate(NULL, 1024));

    // This should still return true.
    EXPECT_TRUE(segment->allMemoryDeallocated());
}

} // anonymous namespace
