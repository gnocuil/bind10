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


#include <config.h>
#include <asiolink/io_address.h>
#include <dhcp/dhcp4o6_ipc.h>
#include <dhcp/dhcp4.h>
#include <dhcp/dhcp6.h>
#include <dhcp/libdhcp++.h>
#include <dhcp/docsis3_option_defs.h>
#include <dhcp/option_string.h>
#include <dhcp/pkt4.h>
#include <dhcp/pkt6.h>
#include <dhcp/option.h>
#include <exceptions/exceptions.h>
#include <util/buffer.h>
#include <dhcp/pkt4o6.h>
#include <dhcp/iface_mgr.h>
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/static_assert.hpp>

#include <gtest/gtest.h>


using namespace std;
using namespace isc;
using namespace isc::asiolink;
using namespace isc::dhcp;
using namespace isc::util;

namespace {

// Make sure the test is large enough and does not fit into one
// read or write request

const int LENGTH = 250;
uint8_t testData[LENGTH];


class DHCP4o6IPCTest : public ::testing::Test {
    public:
        DHCP4IPC ipc4;
        DHCP6IPC ipc6;
        DHCP4o6IPCTest() {
            for (int i = 0; i < LENGTH; ++i)
                testData[i] = i;
        }
        Pkt4o6Ptr generatePkt4o6(){
	        //construct a Pkt4o6
	        Pkt4Ptr pkt4(new Pkt4(testData,LENGTH));
            Pkt6Ptr pkt6(new Pkt6(testData,LENGTH));
            pkt6->setType(DHCPV4_QUERY);
	        pkt6->setRemotePort(546);
            pkt6->setRemoteAddr(IOAddress("fe80::21e:8cff:fe9b:7349"));
            pkt6->setLocalPort(0);
            pkt6->setLocalAddr(IOAddress("ff02::1:2"));
            pkt6->setIndex(2);
            pkt6->setIface("eth0");
	        pkt4->repack();
	        isc::util::OutputBuffer tmp = pkt4->getBuffer();
            OptionBuffer p((uint8_t*)tmp.getData(),
                           (uint8_t*)tmp.getData()+tmp.getLength());
            OptionPtr opt = OptionPtr(new Option(Option::V6, OPTION_DHCPV4_MSG, p));
            pkt6->addOption(opt);
	        Pkt4o6Ptr pkt4o6(new Pkt4o6(pkt6));
	        return pkt4o6;
        }

};

// Test we use DHCP6IPC to send, DHCP4IPC to receive and vice-versa
TEST_F(DHCP4o6IPCTest, v4send_v6receive) {
    //create Pkt4o6
	Pkt4o6Ptr pkt4o6 = generatePkt4o6();

    std::string json = pkt4o6->getJsonAttribute();
    ipc4.open();
    ipc6.open();
    EXPECT_NO_THROW(
       ipc4.sendPkt4o6(pkt4o6); 
    );
    EXPECT_NO_THROW(
       ipc6.sendPkt4o6(pkt4o6); 
    );    
    EXPECT_NO_THROW(
       ipc6.recvPkt4o6();
    );   
    EXPECT_NO_THROW(
       ipc4.recvPkt4o6();
    );   
    Pkt4o6Ptr recvmsg1 = ipc6.pop();
    Pkt4o6Ptr recvmsg2 = ipc4.pop();

    ASSERT_EQ(json, recvmsg1->getJsonAttribute());//json must be equal
    const OutputBuffer &buf4_1(recvmsg1->getPkt4()->getBuffer());
    size_t len = buf4_1.getLength();
    ASSERT_EQ(LENGTH,len);//length of data in pkt4 must be equal
    const OutputBuffer &buf6_1(recvmsg1->getPkt6()->getBuffer());
    len = buf6_1.getLength();
    ASSERT_EQ(LENGTH,len);//length of data in pkt6 must be equal
    
    uint8_t *pkt4data1 = (uint8_t*)buf4_1.getData();
    for(int i = 0;i < LENGTH ;i++){//test data in pkt4
	    EXPECT_EQ(pkt4data1[i],testData[i]);
    }
    uint8_t *pkt6data1 = (uint8_t*)buf6_1.getData();
    for(int i = 0;i < LENGTH ;i++){//test data in pkt6
	    EXPECT_EQ(pkt6data1[i],testData[i]);
    }
    
    ASSERT_EQ(json, recvmsg2->getJsonAttribute());//json must be equal
    const OutputBuffer &buf4_2(recvmsg2->getPkt4()->getBuffer());
    len = buf4_2.getLength();
    ASSERT_EQ(LENGTH,len);//length of data in pkt4 must be equal
    const OutputBuffer &buf6_2(recvmsg2->getPkt6()->getBuffer());
    len = buf6_2.getLength();
    ASSERT_EQ(LENGTH,len);//length of data in pkt6 must be equal
    
    uint8_t *pkt4data2 = (uint8_t*)buf4_2.getData();
    for(int i = 0;i < LENGTH ;i++){//test data in pkt4
	    EXPECT_EQ(pkt4data2[i],testData[i]);
    }
    uint8_t *pkt6data2 = (uint8_t*)buf6_2.getData();
    for(int i = 0;i < LENGTH ;i++){//test data in pkt6
	    EXPECT_EQ(pkt6data2[i],testData[i]);
    }
}

// test send exception
TEST_F(DHCP4o6IPCTest, exception) {
    EXPECT_THROW(
        ipc4.sendPkt4o6(Pkt4o6Ptr()),
        DHCP4o6IPCSendError
    );
}

// test callback function
TEST_F(DHCP4o6IPCTest, callback) {
    Pkt4o6Ptr pkt4o6 = generatePkt4o6();
    ipc4.open();
    ipc6.open();

    ipc4.sendPkt4o6(pkt4o6);//send a packet to ipc6
    EXPECT_EQ(true, ipc6.empty());//ipc6 has not received any packet
    DHCP4o6IPC::callback();//ipc6 defined last, so call ipc6.receivePkt4o6()
    EXPECT_EQ(false, ipc6.empty());//now ipc6 received a packet

    ipc6.pop();//clear
    EXPECT_EQ(true, ipc6.empty());//ipc6 empty
    ipc6.enable();//add external socket into IfaceMgr
    ipc4.sendPkt4o6(pkt4o6);//send a packet to ipc6
    EXPECT_EQ(true, ipc6.empty());//ipc6 has not received any packet
    IfaceMgr::instance().receive4(5);//IfaceMgr receive packets by select()
    EXPECT_EQ(false, ipc6.empty());//now ipc6 should have received a packet
}



}



