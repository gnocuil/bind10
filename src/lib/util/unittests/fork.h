// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef __UTIL_UNITTESTS_FORK_H
#define __UTIL_UNITTESTS_FORK_H 1

#include <config.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <util/unittests/dll.h>

/**
 * @file fork.h
 * @short Help functions to fork the test case process.
 * Various functions to fork a process and feed some data to pipe, check
 * its output and such lives here.
 */

namespace isc {
namespace util {
namespace unittests {

/**
 * @short Checks that a process terminates correctly.
 * Waits for a process to terminate (with a short timeout, this should be
 * used whan the process is about tu terminate) and checks its exit code.
 *
 * @return True if the process terminates with 0, false otherwise.
 * @param process The ID of process to wait for.
 */
B10_LIBUTIL_UNITTESTS_API bool
process_ok(pid_t process);

B10_LIBUTIL_UNITTESTS_API pid_t
provide_input(int *read_pipe, const void *input, const size_t length);

B10_LIBUTIL_UNITTESTS_API pid_t
check_output(int *write_pipe, const void *output, const size_t length);

} // End of the namespace
}
}

#endif // __UTIL_UNITTESTS_FORK_H
