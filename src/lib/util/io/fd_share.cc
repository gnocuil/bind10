// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <process.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <stdlib.h>             // for malloc and free
#include "fd_share.h"

#ifndef _WIN32

/* Unix version */

namespace isc {
namespace util {
namespace io {

namespace {
// Not all OSes support advanced CMSG macros: CMSG_LEN and CMSG_SPACE.
// In order to ensure as much portability as possible, we provide wrapper
// functions of these macros.
// Note that cmsg_space() could run slow on OSes that do not have
// CMSG_SPACE.
inline socklen_t
cmsg_len(const socklen_t len) {
#ifdef CMSG_LEN
    return (CMSG_LEN(len));
#else
    // Cast NULL so that any pointer arithmetic performed by CMSG_DATA
    // is correct.
    const uintptr_t hdrlen = (uintptr_t)CMSG_DATA(((struct cmsghdr*)NULL));
    return (hdrlen + len);
#endif
}

inline socklen_t
cmsg_space(const socklen_t len) {
#ifdef CMSG_SPACE
    return (CMSG_SPACE(len));
#else
    struct msghdr msg;
    struct cmsghdr* cmsgp;
    // XXX: The buffer length is an ad hoc value, but should be enough
    // in a practical sense.
    char dummybuf[sizeof(struct cmsghdr) + 1024];

    memset(&msg, 0, sizeof(msg));
    msg.msg_control = dummybuf;
    msg.msg_controllen = sizeof(dummybuf);

    cmsgp = (struct cmsghdr*)dummybuf;
    cmsgp->cmsg_len = cmsg_len(len);

    cmsgp = CMSG_NXTHDR(&msg, cmsgp);
    if (cmsgp != NULL) {
        return ((char*)cmsgp - (char*)msg.msg_control);
    } else {
        return (0);
    }
#endif  // CMSG_SPACE
}
}

int
recv_fd(const int sock) {
    struct msghdr msghdr;
    struct iovec iov_dummy;
    unsigned char dummy_data;

    iov_dummy.iov_base = &dummy_data;
    iov_dummy.iov_len = sizeof(dummy_data);
    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &iov_dummy;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_controllen = cmsg_space(sizeof(int));
    msghdr.msg_control = malloc(msghdr.msg_controllen);
    if (msghdr.msg_control == NULL) {
        return (FD_SYSTEM_ERROR);
    }

    const int cc = recvmsg(sock, &msghdr, 0);
    if (cc <= 0) {
        free(msghdr.msg_control);
        if (cc == 0) {
            errno = ECONNRESET;
        }
        return (FD_SYSTEM_ERROR);
    }
    const struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msghdr);
    int fd = FD_OTHER_ERROR;
    if (cmsg != NULL && cmsg->cmsg_len == cmsg_len(sizeof(int)) &&
        cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        std::memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    }
    free(msghdr.msg_control);
    // It is strange, but the call can return the same file descriptor as
    // one returned previously, even if that one is not closed yet. So,
    // we just re-number every one we get, so they are unique.
    int new_fd(dup(fd));
    int close_error(close(fd));
    if (close_error == -1 || new_fd == -1) {
        // We need to return an error, because something failed. But in case
        // it was the previous close, we at least try to close the duped FD.
        if (new_fd != -1) {
            close(new_fd); // If this fails, nothing but returning error can't
                           // be done and we are doing that anyway.
        }
        return (FD_SYSTEM_ERROR);
    }
    return (new_fd);
}

int
send_fd(const int sock, const int fd) {
    struct msghdr msghdr;
    struct iovec iov_dummy;
    unsigned char dummy_data = 0;

    iov_dummy.iov_base = &dummy_data;
    iov_dummy.iov_len = sizeof(dummy_data);
    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &iov_dummy;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_controllen = cmsg_space(sizeof(int));
    msghdr.msg_control = malloc(msghdr.msg_controllen);
    if (msghdr.msg_control == NULL) {
        return (FD_OTHER_ERROR);
    }

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = cmsg_len(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    std::memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    const int ret = sendmsg(sock, &msghdr, 0);
    free(msghdr.msg_control);
    return (ret >= 0 ? 0 : FD_SYSTEM_ERROR);
}

} // End for namespace io
} // End for namespace util
} // End for namespace isc


#else

/* Windows version */

namespace isc {
namespace util {
namespace io {

namespace {
// Need the peer PID

int
send_pid(const SOCKET sock) {
    pid_t pid(_getpid());
    if (send(sock, (const char *)&pid, sizeof(pid), 0) != sizeof(pid)) {
        return (FD_SYSTEM_ERROR);
    }
    return (0);
}

pid_t
recv_pid(const SOCKET sock) {
    pid_t pid;
    if (recv(sock, (char *)&pid , sizeof(pid), 0) != sizeof(pid)) {
        return (FD_SYSTEM_ERROR);
    }
    return (pid);
}
}

SOCKET
recv_fd(const SOCKET sock) {
    int ret(send_pid(sock));
    if (ret != 0) {
        return ((SOCKET) ret);
    }
    WSAPROTOCOL_INFO pi;
    if (recv(sock, (char *)&pi, sizeof(pi), 0) != sizeof(pi)) {
        return ((SOCKET) FD_SYSTEM_ERROR);
    }
    SOCKET nsock = WSASocket(pi.iAddressFamily,
                             pi.iSocketType,
                             pi.iProtocol,
                             &pi, 0, 0);
    if (nsock == INVALID_SOCKET) {
        return ((SOCKET) FD_OTHER_ERROR);
    }
    return (nsock);
}

int
send_fd(const SOCKET sock, const SOCKET fd) {
    pid_t peerpid(recv_pid(sock));
    if (peerpid == FD_SYSTEM_ERROR) {
        return (FD_SYSTEM_ERROR);
    }

    WSAPROTOCOL_INFO pi;
    if (WSADuplicateSocket(fd, peerpid, &pi) != 0) {
        return (FD_OTHER_ERROR);
    }

    if (send(sock, (const char *)&pi, sizeof(pi), 0) != sizeof(pi)) {
        return (FD_SYSTEM_ERROR);
    }
    return (0);

}

} // End for namespace io
} // End for namespace util
} // End for namespace isc

#endif
