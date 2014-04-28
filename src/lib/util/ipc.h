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

#include <buffer.h>

#include <sys/un.h>
#include <stdio.h>

namespace isc {
namespace util {

class IPCBindError : public Exception {
public:
    IPCBindError(const char* file, size_t line, const char* what) :
    isc::Exception(file, line, what) { };
};

class BaseIPC {
public:

    /// @brief Packet reception buffer size
    ///
    /// receive buffer size of UNIX socket
    static const uint32_t RCVBUFSIZE = 4096;

    /// @brief BaseIPC constructor.
    ///
    /// Creates BaseIPC object that represents UNIX communication.
    BaseIPC() :
        socketfd_(-1),
        remote_addr_len_(0),
        local_addr_len_(0)
    {
    }

    /// @brief BaseIPC destructor.
    ///
    /// Delete a BaseIPC object.
    ~BaseIPC() { closeSocket(); }
    
    
    /// @brief Create UNIX socket
    ///
    /// Method will throw if socket creation fails.
    ///
    /// @return socket descriptor
    int openSocket() {
        //create socket
        int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (fd < 0) {
        perror ("socket() failed");
        exit(EXIT_FAILURE);
    }
        return socketfd_ = fd;
    }

    /// @brief bind UNIX socket to the given filename
    ///
    /// @param local_name local filename, will bind to this address
    ///
    /// Method will throw if socket binding fails.
    void bindSocket(const std::string& local_name) {
        if (socketfd_ < 0)
            openSocket();
        //init address
        memset(&local_addr_, 0, sizeof(struct sockaddr_un));
        local_addr_.sun_family = AF_UNIX;
        strcpy(&local_addr_.sun_path[1], local_name.c_str());
        local_addr_len_ = sizeof(sa_family_t) + local_name.size() + 1;

        //bind to local_address
        if (bind(socketfd_, (struct sockaddr *)&local_addr_, local_addr_len_) < 0) {
        isc_throw(IPCBindError, "failed to bind to local_address");
    }
    }

    /// @brief set remote filename
    ///
    /// @param remote_name Remote filename
    void setRemote(const std::string& remote_name) {
        if (socketfd_ < 0)
            openSocket();
        //init address
        memset(&remote_addr_, 0, sizeof(struct sockaddr_un));
        remote_addr_.sun_family = AF_UNIX;
        strcpy(&remote_addr_.sun_path[1], remote_name.c_str());
        remote_addr_len_ = sizeof(sa_family_t) + remote_name.size() + 1;
    }

    /// @brief Close opened socket.
    void closeSocket() {
        if(socketfd_ > 0)
            close(socketfd_);
        socketfd_ = -1;
    }

    /// @brief Send message to a host that the same socket are binding. 
    /// 
    /// @param buf the date are prepared to send.
    ///
    /// Method will throw if setRemote() has not been called
    ///
    /// @return the number of data have been sent.
    int send(const isc::util::OutputBuffer &buf) { 
        //TODO: check if connect() has been called
        if (connect(socketfd_, (struct sockaddr *)&remote_addr_, remote_addr_len_) < 0) {
        perror ("connect()failed");
        exit(EXIT_FAILURE);
    }
        int count = sendto(socketfd_, buf.getData(), buf.getLength(), 0,
                           (struct sockaddr*)&remote_addr_, remote_addr_len_);
        if (count < 0) {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    } 
        return count;
    }

    /// @brief receive message from a host that the same socket are binding.
    ///
    /// Method will throw if bindSocket() has not been called 
    ///
    /// @return the data have been received.
    isc::util::InputBuffer recv() {
        //TODO: check if bind() has been called
        uint8_t buf[RCVBUFSIZE];
        int len = recvfrom(socketfd_, buf, RCVBUFSIZE, 0, NULL, NULL);
        if (len < 0) {
         perror("recvfrom() failed");
         exit(EXIT_FAILURE);
    } 
        isc::util::InputBuffer ibuf(buf, len);
        return ibuf;
    }

    /// @brief get socket value.
    /// 
    /// @return the socket value this class is using.
    int getSocket() { return socketfd_; }

    /// @briefReturns value of transaction-id field
    ///
    /// @return transaction-id
    ///uint32_t getTransid() const { return (transid_); };

protected:
	
    /// UNIX socket value.
    int socketfd_;
    
    ///remote UNIX socket address 
    struct sockaddr_un remote_addr_;
    
    ///length of remote_addr_
    int remote_addr_len_;

    ///local UNIX socket address
    struct sockaddr_un local_addr_;
    
    ///length of local_addr_
    int local_addr_len_;
}; // BaseIPC class

} // namespace util
} // namespace isc
#endif  // IPC_H

