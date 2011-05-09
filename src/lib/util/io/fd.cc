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

#include "fd.h"

#ifdef _WIN32
#include <io.h>
#define write _write
#define read _read
#endif

#include <cerrno>

namespace isc {
namespace util {
namespace io {

bool
write_data(const int fd, const void *buffer_v, const size_t length) {
    const unsigned char *buffer(static_cast<const unsigned char *>(buffer_v));
    size_t rest(length);
    // Just keep writing until all is written
    while (rest) {
        ssize_t written(write(fd, buffer, rest));
        if (rest == -1) {
            if (errno == EINTR) { // Just keep going
                continue;
            } else {
                return false;
            }
        } else { // Wrote something
            rest -= written;
            buffer += written;
        }
    }
    return true;
}

ssize_t
read_data(const int fd, void *buffer_v, const size_t length) {
    unsigned char *buffer(static_cast<unsigned char *>(buffer_v));
    size_t rest(length), already(0);
    while (rest) { // Stil something to read
        ssize_t amount(read(fd, buffer, rest));
        if (rest == -1) {
            if (errno == EINTR) { // Continue on interrupted call
                continue;
            } else {
                return -1;
            }
        } else if (amount) {
            already += amount;
            rest -= amount;
            buffer += amount;
        } else { // EOF
            return already;
        }
    }
    return already;
}

}
}
}
