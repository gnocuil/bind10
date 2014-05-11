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

//Use private codes now, waiting for IANA to assign codes and move to dhcp6.h
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
    Pkt4o6(const uint8_t* data4, size_t len4,
           const uint8_t* data6, size_t len6, std::string json);
/// @Constructor, Returns 4o6 packet decapsulated from v6 packet.
///
/// @param pkt received v6 packet.
    Pkt4o6(const Pkt6Ptr& pkt6);               
    
    Pkt4o6(const Pkt4o6Ptr& pkt4o6, const Pkt4Ptr& pkt4);

    Pkt6Ptr getPkt6() const { return (pkt6_); }
    Pkt4Ptr getPkt4() const { return (pkt4_); }
    
    std::string getJson();
    void setJson(std::string json);
    
    OptionBuffer getDHCPv4MsgOption();
    
protected:
    void setPkt4LocalAddr();
    
    Pkt4Ptr pkt4_;
    Pkt6Ptr pkt6_;    
};// pkt4o6 class

} // isc::dhcp namespace

} // isc namespace      
    
#endif
   
    
     
       
             
             
      
