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

#define ISC_LIBUTIL_UNITTESTS_EXPORT

#include <config.h>

#include "resource.h"

#include <gtest/gtest.h>

#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif

namespace isc {
namespace util {
namespace unittests {

ISC_LIBUTIL_UNITTESTS_API void
dontCreateCoreDumps() {
#ifndef _WIN32
    const rlimit core_limit = {0, 0};

    EXPECT_EQ(setrlimit(RLIMIT_CORE, &core_limit), 0);
#endif
}

} // end of namespace unittests
} // end of namespace util
} // end of namespace isc
