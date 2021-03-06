// Copyright (C) 2011-2013 Internet Systems Consortium, Inc. ("ISC")
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

#include <dhcp/dhcp6.h>
#include <dhcp/libdhcp++.h>
#include <dhcp/option6_ia.h>
#include <exceptions/exceptions.h>
#include <util/io_utilities.h>

#include <arpa/inet.h>
#include <sstream>
#include <stdint.h>

using namespace std;
using namespace isc::util;

namespace isc {
namespace dhcp {

Option6IA::Option6IA(uint16_t type, uint32_t iaid)
    :Option(Option::V6, type), iaid_(iaid), t1_(0), t2_(0) {

    // IA_TA has different layout than IA_NA and IA_PD. We can't sue this class
    if (type == D6O_IA_TA) {
        isc_throw(BadValue, "Can't use Option6IA for IA_TA as it has "
                  "a different layout");
    }

    setEncapsulatedSpace("dhcp6");
}

Option6IA::Option6IA(uint16_t type, OptionBufferConstIter begin,
                     OptionBufferConstIter end)
    :Option(Option::V6, type) {

    // IA_TA has different layout than IA_NA and IA_PD. We can't use this class
    if (type == D6O_IA_TA) {
        isc_throw(BadValue, "Can't use Option6IA for IA_TA as it has "
                  "a different layout");
    }

    setEncapsulatedSpace("dhcp6");

    unpack(begin, end);
}

void Option6IA::pack(isc::util::OutputBuffer& buf) {
    buf.writeUint16(type_);
    buf.writeUint16(len() - OPTION6_HDR_LEN);
    buf.writeUint32(iaid_);
    buf.writeUint32(t1_);
    buf.writeUint32(t2_);

    packOptions(buf);
}

void Option6IA::unpack(OptionBufferConstIter begin,
                       OptionBufferConstIter end) {
    // IA_NA and IA_PD have 12 bytes content (iaid, t1, t2 fields)
    // followed by 0 or more sub-options.
    if (distance(begin, end) < OPTION6_IA_LEN) {
        isc_throw(OutOfRange, "Option " << type_ << " truncated");
    }
    iaid_ = readUint32(&(*begin), distance(begin, end));
    begin += sizeof(uint32_t);
    t1_ = readUint32(&(*begin), distance(begin, end));
    begin += sizeof(uint32_t);

    t2_ = readUint32(&(*begin), distance(begin, end));
    begin += sizeof(uint32_t);

    unpackOptions(OptionBuffer(begin, end));
}

std::string Option6IA::toText(int indent /* = 0*/) {
    stringstream tmp;

    for (int i=0; i<indent; i++)
        tmp << " ";
    tmp << "type=" << type_;

    switch (type_) {
    case D6O_IA_NA:
        tmp << "(IA_NA)";
        break;
    case D6O_IA_PD:
        tmp << "(IA_PD)";
        break;
    default:
        tmp << "(unknown)";
    }
    tmp << " iaid=" << iaid_ << ", t1=" << t1_ << ", t2=" << t2_
        << " " << options_.size() << " sub-options:" << endl;

    for (OptionCollection::const_iterator opt=options_.begin();
         opt!=options_.end();
         ++opt) {
        tmp << (*opt).second->toText(indent+2);
    }
    return tmp.str();
}

uint16_t Option6IA::len() {

    uint16_t length = OPTION6_HDR_LEN /*header (4)*/ +
        OPTION6_IA_LEN  /* option content (12) */;

    // length of all suboptions
    for (OptionCollection::iterator it = options_.begin();
         it != options_.end();
         ++it) {
        length += (*it).second->len();
    }
    return (length);
}

} // end of isc::dhcp namespace
} // end of isc namespace
