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

class DHCP4o6_IPC : public ::testing::Test {
    public:
        DHCP4o6_IPC(){

        }

};

// Test we use DHCP6IPC to send, DHCP4IPC to receive
TEST_F(DHCP4o6_IPC, v4send_v6receive) {

    //create Pkt4o6
    uint8_t data[] = { 0, 1, 2, 3, 4, 5 };
    Pkt6Ptr pktv6(new Pkt6(data, sizeof(data)));
    pktv6->setRemotePort(546);
    pktv6->setRemoteAddr(IOAddress("fe80::21e:8cff:fe9b:7349"));
    pktv6->setLocalPort(0);
    pktv6->setLocalAddr(IOAddress("ff02::1:2"));
    pktv6->setIndex(2);
    pktv6->setIface("eth0");
    uint8_t testData[250];
    for (int i = 0; i < 250; i++) {
        testData[i] = i;
    }
    
    Pkt4Ptr pktv4(new Pkt4(testData,250));
    pktv4->repack();
    isc::util::OutputBuffer tmp = pktv4->getBuffer();
    OptionBuffer opt_buf((uint8_t*)tmp.getData(),
	                     (uint8_t*)tmp.getData()+tmp.getLength());
    OptionPtr opt = OptionPtr(new Option(Option::V6, 
                                        OPTION_DHCPV4_MSG, opt_buf));
    pktv6->addOption(opt);
	Pkt4o6Ptr pkt4o6(new Pkt4o6(pktv6));
	//end of create Pkt4o6

    std::string json = pkt4o6->getJson();
    DHCP4IPC ipc4;
    DHCP6IPC ipc6;
    ipc4.open();
    ipc6.open();
    ipc4.sendPkt4o6(pkt4o6);
    ipc6.recvPkt4o6();
    Pkt4o6Ptr recvmsg = ipc6.pop();
    ipc4.closeSocket();
    ipc6.closeSocket();
    ASSERT_EQ(json, recvmsg->getJson());//josn must be equal
    const OutputBuffer &buf4(recvmsg->getPkt4()->getBuffer());
    size_t len = buf4.getLength();
    ASSERT_EQ(250,len);//length of data in pkt4 must be equal
    const OutputBuffer &buf6(recvmsg->getPkt6()->getBuffer());
    len = buf6.getLength();
    ASSERT_EQ(6,len);//length of data in pkt6 must be equal
    
    uint8_t *pkt4data = (uint8_t*)buf4.getData();
    for(int i = 0;i < 250 ;i++){//test data in pkt4
	    EXPECT_EQ(pkt4data[i],testData[i]);
    }
    uint8_t *pkt6data = (uint8_t*)buf6.getData();
    for(int i = 0;i < 6 ;i++){//test data in pkt6
	    EXPECT_EQ(pkt6data[i],data[i]);
    }
}


// Test we use DHCP4IPC to send, DHCP6IPC to receive
TEST_F(DHCP4o6_IPC, v6send_v4receive) {
  
    //create Pkt4o6
    uint8_t data[] = { 0, 1, 2, 3, 4, 5 };
    Pkt6Ptr pktv6(new Pkt6(data, sizeof(data)));
    pktv6->setRemotePort(546);
    pktv6->setRemoteAddr(IOAddress("fe80::21e:8cff:fe9b:7349"));
    pktv6->setLocalPort(0);
    pktv6->setLocalAddr(IOAddress("ff02::1:2"));
    pktv6->setIndex(2);
    pktv6->setIface("eth0");
    
    //Create Pkt4
    uint8_t testData[250];
    for (int i = 0; i < 250; i++) {
        testData[i] = i;
    }
    Pkt4Ptr pktv4(new Pkt4(testData,250));
    pktv4->repack();
    isc::util::OutputBuffer tmp = pktv4->getBuffer();
    OptionBuffer opt_buf((uint8_t*)tmp.getData(),
	                     (uint8_t*)tmp.getData()+tmp.getLength());
    OptionPtr opt = OptionPtr(new Option(Option::V6, 
                                        OPTION_DHCPV4_MSG, opt_buf));
    pktv6->addOption(opt);
	Pkt4o6Ptr pkt4o6(new Pkt4o6(pktv6));
	//end of create Pkt4o6
    std::string josn = pkt4o6->getJson();
    DHCP4IPC ipcv4;
    DHCP6IPC ipcv6;
    ipcv4.open();
    ipcv6.open();
    ipcv6.sendPkt4o6(pkt4o6);
    ipcv4.recvPkt4o6(); 
    Pkt4o6Ptr recvmsg = ipcv4.pop();
    ipcv4.closeSocket();
    ipcv6.closeSocket();
    ASSERT_EQ(josn, recvmsg->getJson());//josn must be equal
    
    const OutputBuffer &buf4(recvmsg->getPkt4()->getBuffer());
    size_t len = buf4.getLength();
    ASSERT_EQ(250,len);//length of data in pkt4 must be equal
    const OutputBuffer &buf6(recvmsg->getPkt6()->getBuffer());
    len = buf6.getLength();
    ASSERT_EQ(6,len);//length of data in pkt6 must be equal
    uint8_t *pkt4data = (uint8_t*)buf4.getData();
    for(int i = 0;i < 250 ;i++){
	    EXPECT_EQ(pkt4data[i],testData[i]);
    }
    uint8_t *pkt6data = (uint8_t*)buf6.getData();
    for(int i = 0;i < 6 ;i++){
	    EXPECT_EQ(pkt6data[i],data[i]);
    }
}

TEST_F(DHCP4o6_IPC, exceptiontest){
    //we use a empty Pkt4o6 to be sent,expect a exception
    DHCP4IPC ipc4;
    ipc4.open();
    const Pkt4o6Ptr pkt4o6;
    
    //pkt4o6 is empty, we should throw.
    EXPECT_ANY_THROW(ipc4.sendPkt4o6(pkt4o6));
    
    DHCP6IPC ipc6;
    ipc6.open();
    const Pkt4o6Ptr pkt4o6_ = ipc6.pop();//queue in ipc6 is empty now
    EXPECT_EQ(true,ipc6.empty());
    EXPECT_ANY_THROW(ipc6.sendPkt4o6(pkt4o6_));
    
}

}



