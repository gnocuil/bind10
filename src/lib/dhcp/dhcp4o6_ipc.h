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

class DHCP4o6IPC : public isc::util::BaseIPC {
public:
    DHCP4o6IPC() { instance_ = this; }
    virtual ~DHCP4o6IPC() { instance_ = NULL; }
    void open();
    void sendPkt4o6(const Pkt4o6Ptr& pkt4o6);
    void recvPkt4o6();
    static void callback();
    bool empty() const { return queue_.empty(); }
    Pkt4o6Ptr pop();
    bool isCurrent(Pkt4Ptr pkt4) {
        return current_ && pkt4 == current_->getPkt4();
    }
    bool isCurrent(Pkt6Ptr pkt6) {
        return current_ && pkt6 == current_->getPkt6();
    }
    Pkt4o6Ptr current() { return current_; }
    static void enable();
    static void disable();
protected:
    virtual std::string getLocalFilename() const = 0;
    virtual std::string getRemoteFilename() const = 0;
    std::queue<Pkt4o6Ptr> queue_;
    
    /// @brief Static pointer to the sole instance of the DHCP4o6 IPC.
    ///
    /// This is required for receive callback in iface_mgr
    static DHCP4o6IPC* instance_;
    Pkt4o6Ptr current_;
};//DHCP4o6IPC class

#define FILENAME_4TO6 "DHCPv4_over_DHCPv6_v4tov6";
#define FILENAME_6TO4 "DHCPv4_over_DHCPv6_v6tov4";

class DHCP4IPC : public DHCP4o6IPC {
protected:
    virtual std::string getLocalFilename() const { return FILENAME_6TO4; }
    virtual std::string getRemoteFilename() const { return FILENAME_4TO6; }
};

class DHCP6IPC : public DHCP4o6IPC {
protected:
    virtual std::string getLocalFilename() const { return FILENAME_4TO6; }
    virtual std::string getRemoteFilename() const { return FILENAME_6TO4; }
};

typedef boost::shared_ptr<DHCP4o6IPC> DHCP4o6IPCPtr;

} // isc::dhcp namespace
} // isc namespace

#endif //DHCP4O6_IPC_H
