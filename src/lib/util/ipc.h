// Copyright (C) 2014 Internet Systems Consortium, Inc. ("ISC")
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

#ifndef IPC_H
#define IPC_H

#include <util/buffer.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <errno.h>

namespace isc {
namespace util {

/// @brief Exception thrown when BaseIPC bindSocket() failed.
class IPCBindError : public Exception {
public:
    IPCBindError(const char* file, size_t line, const char* what) :
    isc::Exception(file, line, what) { };
};

/// @brief Exception thrown when BaseIPC openSocket() failed.
class IPCSocketError : public Exception {
public:
    IPCSocketError(const char* file, size_t line, const char* what) :
    isc::Exception(file, line, what) { };
};

/// @brief Exception thrown when BaseIPC recv() failed.
class IPCRecvError : public Exception {
public:
    IPCRecvError(const char* file, size_t line, const char* what) :
    isc::Exception(file, line, what) { };
};

/// @brief Exception thrown when BaseIPC send() failed.
class IPCSendError : public Exception {
public:
    IPCSendError(const char* file, size_t line, const char* what) :
    isc::Exception(file, line, what) { };
};

/// @brief IPC tool based on UNIX domain socket
class BaseIPC {
public:

    /// @brief Packet reception buffer size
    ///
    /// receive buffer size of UNIX socket
    static const uint32_t RCVBUFSIZE = 4096;

    /// @brief BaseIPC constructor.
    ///
    /// Creates BaseIPC object that represents UNIX communication.
    BaseIPC(const std::string& local_filename, const std::string& remote_filename) :
        socketfd_(-1),
        remote_addr_len_(0),
        local_filename_(local_filename),
        remote_filename_(remote_filename)
    {
    }

    /// @brief BaseIPC destructor.
    ///
    /// Delete a BaseIPC object.
    virtual ~BaseIPC() { closeIPC(); }
    
    
    /// @brief Open UNIX socket
    ///
    /// Method will throw if socket creation fails.
    ///
    /// @return socket descriptor
    int open() {
        //create socket
        int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (fd < 0) {
            isc_throw(IPCSocketError, "BaseIPC failed to creat a socket");
    	}
    	socketfd_ = fd;
    	
        bindSocket();
        setRemote();
    	
    	return socketfd_;
    }

    /// @brief Close opened socket.
    void closeIPC() {
        if(socketfd_ >= 0)
            close(socketfd_);
        socketfd_ = -1;
    }

    /// @brief Send message to a host that the same socket are binding. 
    /// 
    /// @param buf the date are prepared to send.
    ///
    /// setRemote() MUST be called before calling this function
    /// Method will throw if setRemote() has not been called
    /// or sendto() failed
    ///
    /// @return the number of data have been sent.
    int send(const isc::util::OutputBuffer &buf) { 
        if (remote_addr_len_ == 0) {
            isc_throw(IPCSendError, "Remote address unset, call setRemote() first");
        }
        int count = sendto(socketfd_, buf.getData(), buf.getLength(), 0,
                           (struct sockaddr*)&remote_addr_, remote_addr_len_);
        if (count < 0) {
            isc_throw(IPCSendError, "BaseIPC failed on sendto: "
                                    << strerror(errno));
        }
        return count;
    }

    /// @brief receive message from a host that the same socket are binding.
    ///
    /// bindSocket() MUST be called before calling this function
    /// Method will throw if bindSocket() has not been called
    /// or recvfrom() failed
    ///
    /// @return the data have been received.
    isc::util::InputBuffer recv() {
        uint8_t buf[RCVBUFSIZE];
        int len = recvfrom(socketfd_, buf, RCVBUFSIZE, 0, NULL, NULL);
        if (len < 0) {
            isc_throw(IPCRecvError, "BaseIPC failed on recvfrom: "
                                    << strerror(errno));
    	} 
        isc::util::InputBuffer ibuf(buf, len);
        return ibuf;
    }

    /// @brief get socket value.
    /// 
    /// @return the socket value this class is using.
    int getSocket() { return socketfd_; }

protected:

    /// @brief set remote filename
    void setRemote() {
        //init address
        memset(&remote_addr_, 0, sizeof(struct sockaddr_un));
        remote_addr_.sun_family = AF_UNIX;
        strcpy(&remote_addr_.sun_path[1], remote_filename_.c_str());
        remote_addr_len_ = sizeof(sa_family_t) + remote_filename_.size() + 1;
    }
    
    /// @brief bind UNIX socket to the given filename
    ///
    /// Method will throw if socket binding fails.
    void bindSocket() {
        struct sockaddr_un local_addr_;
        int local_addr_len_;
            
        //init address
        memset(&local_addr_, 0, sizeof(struct sockaddr_un));
        local_addr_.sun_family = AF_UNIX;
        strcpy(&local_addr_.sun_path[1], local_filename_.c_str());
        local_addr_len_ = sizeof(sa_family_t) + local_filename_.size() + 1;

        //bind to local_address
        if (bind(socketfd_, (struct sockaddr *)&local_addr_, local_addr_len_) < 0) {
            isc_throw(IPCBindError, "failed to bind to local address: " + local_filename_);
    	}
    }
    
    /// UNIX socket value.
    int socketfd_;
    
    ///remote UNIX socket address 
    struct sockaddr_un remote_addr_;
    
    ///length of remote_addr_
    int remote_addr_len_;

    /// filename for receiving and sending socket
    std::string local_filename_, remote_filename_;

}; // BaseIPC class

} // namespace util
} // namespace isc
#endif  // IPC_H

