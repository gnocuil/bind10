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

#ifndef PKT4o6_H
#define PKT4o6_H

#include <dhcp/pkt6.h>
#include <dhcp/pkt4.h>
#include <dhcp/option.h>

//Use private numbers now, waiting for IANA to assign codes and move to dhcp6.h
#define OPTION_DHCPV4_MSG         54321 /* 4o6: DHCPv4 Message Option */
#define DHCPV4_QUERY              245 /* 4o6: DHCPv4-qurey message */
#define DHCPV4_RESPONSE           246 /* 4o6: DHCPv4-response message*/

namespace isc {

namespace dhcp {

/// @brief Pkt4o6 exception thrown when construction fails.
class Pkt4o6ConstructError : public Exception {
public:
    Pkt4o6ConstructError(const char* file, size_t line, const char* what) :
        isc::Exception(file, line, what) { };
};

class Pkt4o6;
typedef boost::shared_ptr<Pkt4o6> Pkt4o6Ptr;

class Pkt4o6{
public:
    /// @brief Constructor, used in DHCP4o6IPC to construct Pkt4o6 from
    /// raw data
    ///
    /// will construct Pkt4 and Pkt6 seperately 
    ///
    /// @param data4 raw data of Pkt4
    /// @param len4 raw data length of Pkt4
    /// @param data6 raw data of Pkt6
    /// @param len6 raw data length of Pkt6
    Pkt4o6(const uint8_t* data4, size_t len4,
           const uint8_t* data6, size_t len6);
           
    /// @brief Constructor, used in DHCPv6 server to construct Pkt4o6 from
    /// Pkt6
    ///
    /// will construct Pkt4 according to pkt6's DHCPv4 Message Option
    /// 
    /// @throw Pkt4o6ConstructError if DHCPv4 Message Option not found
    ///
    /// @param pkt6 A DHCPv6 DHCPV4_QUERY message
    Pkt4o6(const Pkt6Ptr& pkt6);               

    /// @brief Constructor, used in DHCPv4 server to construct Pkt4o6 from
    /// response Pkt4 and query Pkt4o6
    ///
    /// @param pkt4o6 a DHCPv4 over DHCPv6 message
    /// @param pkt4 a DHCPv4 response message
    Pkt4o6(const Pkt4o6Ptr& pkt4o6, const Pkt4Ptr& pkt4);

    /// @brief get Pkt6 format of this DHCPv4 over DHCPv6 packet
    Pkt6Ptr getPkt6() const { return (pkt6_); }
    
    /// @brief get Pkt4 content of this DHCPv4 over DHCPv6 packet
    Pkt4Ptr getPkt4() const { return (pkt4_); }
    
    /// @brief json format of nessary information
    ///
    /// There are some nessary information that needs to be passed between
    /// DHCPv4 and DHCPv6 servers, such as IPv6 addresses, ports, iface, etc.
    /// The raw data of the DHCPv6 packet does not contain these information,
    /// so we transmit them using json format string
    ///
    /// @return a json format string
    std::string getJsonAttribute();
    
    /// @brief set Packet attributes according to a json format string
    void setJsonAttribute(std::string json);
    
    /// @brief get a DHCPv4MsgOption that contains pkt4_
    ///
    /// @return a DHCPv4MsgOption
    OptionBuffer getDHCPv4MsgOption();
    
    /// @brief set local address in pkt4_ according to U flag in pkt6_
    void setPkt4LocalAddr();
    
protected:
    /// @brief a poiner to a DHCPv4 packet
    Pkt4Ptr pkt4_;
    
    /// @brief a pointer to a DHCPv6 packet
    Pkt6Ptr pkt6_;    
};// pkt4o6 class

} // isc::dhcp namespace

} // isc namespace      
    
#endif
   
    
     
       
             
             
      
