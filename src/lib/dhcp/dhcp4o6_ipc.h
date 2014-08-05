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

#ifndef DHCP4O6_IPC_H
#define DHCP4O6_IPC_H

#include <dhcp/pkt4o6.h>
#include <util/ipc.h>

#include <queue>

namespace isc {
namespace dhcp {

/// @brief Pkt4o6 exception thrown when construction fails.
class DHCP4o6IPCSendError : public Exception {
public:
    DHCP4o6IPCSendError(const char* file, size_t line, const char* what) :
        isc::Exception(file, line, what) { };
};

/// @brief IPC class to pass Pkt4o6 between DHCPv4 and DHCPv6 servers
class DHCP4o6IPC : public isc::util::BaseIPC {
public:
    /// @brief Default constructor.
    ///
    /// DHCP4o6IPC is expected to create only one instance in a process,
    /// and will set itself to intances_ in constructor
    DHCP4o6IPC() { instance_ = this; }
    
    /// @brief Destructor.
    virtual ~DHCP4o6IPC() { instance_ = NULL; }
    
    /// @brief Create and initialize sockets to corresponding addresses
    ///
    /// This function calls methods in BaseIPC for socket processing
    /// Method will throw if BaseIPC methods failed
    void open();
    
    /// @brief Send a DHCPv4 ove DHCPv6 packet
    ///
    /// This function converts Pkt4o6 into binary data and sends it
    /// through BaseIPC send() 
    /// Method will throw if BaseIPC send() failed
    ///
    /// @param pkt4o6 Pointer of the packet to be sent
    void sendPkt4o6(const Pkt4o6Ptr& pkt4o6);
    
    /// @brief Receive a DHCPv4 ove DHCPv6 packet
    ///
    /// This function calls BaseIPC recv() to receive binary data
    /// and converts it into Pkt4o6
    /// Received Pkt4o6 will be push into a queue and not returned
    /// Method will throw if BaseIPC recv() failed or Pkt4o6
    /// construction failed
    void recvPkt4o6();
    
    /// @brief Test if receive queue is empty
    /// 
    /// @return true if queue is empty
    bool empty() const { return queue_.empty(); }
    
    /// @brief Retrive and remove a Pkt4o6 in the receive queue
    ///
    /// @return if not empty, Pkt4o6 in the head of the queue; otherwise
    /// return a null pointer
    Pkt4o6Ptr pop();
    
    /// @brief Check if a given pkt4 is the current processing pkt4o6
    bool isCurrent(Pkt4Ptr pkt4) {
        return (current_ && pkt4 == current_->getPkt4());
    }
    
    /// @brief Check if a given pkt6 is the current processing pkt4o6
    bool isCurrent(Pkt6Ptr pkt6) {
        return (current_ && pkt6 == current_->getPkt6());
    }
    
    /// @brief Get current DHCP4o6IPC instance
    Pkt4o6Ptr current() { return current_; }

protected:

    /// @brief A (local) filename to listen to
    ///
    /// @return a string of filename
    virtual std::string getLocalFilename() const = 0;
    
    /// @brief A (remote) filename to send packets to
    ///
    /// @return a string of filename
    virtual std::string getRemoteFilename() const = 0;
    
    /// @brief A queue of received DHCPv4 over DHCPv6 packets that has
    /// not been processed
    std::queue<Pkt4o6Ptr> queue_;
    
    /// @brief Static pointer to the sole instance of the DHCP4o6 IPC.
    ///
    /// This is required by callback function of iface_mgr
    static DHCP4o6IPC* instance_;
  
    /// @brief The current processing DHCPv4 over DHCPv6 packet
    Pkt4o6Ptr current_;
};//DHCP4o6IPC class

// A filename used for DHCPv4 server --> DHCPv6 server
#define FILENAME_4TO6 "DHCPv4_over_DHCPv6_v4tov6"

// A filename used for DHCPv4 server <-- DHCPv6 server
#define FILENAME_6TO4 "DHCPv4_over_DHCPv6_v6tov4"

/// @brief IPC class actually used in DHCPv4 server
class DHCP4IPC : public DHCP4o6IPC {
protected:
    virtual std::string getLocalFilename() const { return FILENAME_6TO4; }
    virtual std::string getRemoteFilename() const { return FILENAME_4TO6; }
};

/// @brief IPC class actually used in DHCPv6 server
class DHCP6IPC : public DHCP4o6IPC {
protected:
    virtual std::string getLocalFilename() const { return FILENAME_4TO6; }
    virtual std::string getRemoteFilename() const { return FILENAME_6TO4; }
};

typedef boost::shared_ptr<DHCP4o6IPC> DHCP4o6IPCPtr;

} // isc::dhcp namespace
} // isc namespace

#endif //DHCP4O6_IPC_H
