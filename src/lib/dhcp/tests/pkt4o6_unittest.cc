


// Copyright (C) 2011-2013  Internet Systems Consortium, Inc. ("ISC")
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

#include <iostream>
#include <sstream>

#include <arpa/inet.h>

using namespace std;
using namespace isc;
using namespace isc::asiolink;
using namespace isc::dhcp;
using namespace isc::util;
// Don't import the entire boost namespace.  It will unexpectedly hide uint8_t
// for some systems.
using boost::scoped_ptr;

namespace {


class Pkt4o6Test : public ::testing::Test {
public:
    Pkt4o6Test() {

    }
};

//test Pkt4o6 class constructor
TEST_F(Pkt4o6Test, constructor) {


    // test data array
    uint8_t testData[250];
    for (int i = 0; i < 250; i++) {
        testData[i] = i;
    }
    //Case 1:test Pkt4o6::Pkt4o6(const Pkt6Ptr& pkt6)
	//construct a Pkt4o6
	Pkt4Ptr pktv4(new Pkt4(testData,250));
    Pkt6Ptr pktv6(new Pkt6(testData,240));
	pktv6->setIndex(1);
	pktv4->repack();
	isc::util::OutputBuffer tmp = pktv4->getBuffer();
    OptionBuffer p((uint8_t*)tmp.getData(),
                   (uint8_t*)tmp.getData()+tmp.getLength());
    OptionPtr opt = OptionPtr(new Option(Option::V6, OPTION_DHCPV4_MSG, p));
    pktv6->addOption(opt);
	Pkt4o6Ptr pkt4o6(new Pkt4o6(pktv6));
	//end of construct a Pkt4o6

	//now we test original data,create Pkt4 and Pkt6
 	const isc::util::OutputBuffer pkt4_buf = pkt4o6->getPkt4()->getBuffer();
    const isc::util::OutputBuffer pkt6_buf = pkt4o6->getPkt6()->getBuffer();
	Pkt4Ptr v4(new Pkt4((uint8_t*)pkt4_buf.getData(),pkt4_buf.getLength()));
	Pkt6Ptr v6(new Pkt6((uint8_t*)pkt6_buf.getData(),pkt6_buf.getLength()));
	v4->repack();
	v6->repack();
    const isc::util::OutputBuffer &buf4(v4->getBuffer());
    const isc::util::OutputBuffer &buf6(v6->getBuffer());
	ASSERT_EQ(250, buf4.getLength());
	ASSERT_EQ(240, buf6.getLength());//test current length
	uint8_t* data = (uint8_t*)buf4.getData();
	for(int i = 0;i < 10;i++){
		EXPECT_EQ(i,data[i]);
	}

	//Case 2: test Pkt4o6::Pkt4o6(const uint8_t* data4, size_t len4, 
	//                            const uint8_t* data6, size_t len6)
	Pkt4o6Ptr pkt4o6_(new Pkt4o6(testData,240,testData,250));
	ASSERT_EQ(240, pkt4o6_->getPkt4()->getBuffer().getLength());
	ASSERT_EQ(250, pkt4o6_->getPkt6()->getBuffer().getLength());
	data = (uint8_t*)pkt4o6_->getPkt4()->getBuffer().getData();
	for(int i = 0;i < 10;i++){
	    EXPECT_EQ(i,data[i]);
	}
	data = (uint8_t*)pkt4o6_->getPkt6()->getBuffer().getData();
	for(int i = 0;i < 10;i++){
	    EXPECT_EQ(i,data[i]);
	}
	
	//Case 3:test Pkt4o6::Pkt4o6(const Pkt4o6Ptr& pkt4o6, const Pkt4Ptr& pkt4)
	uint8_t newData[240];
	for(int i = 0 ;i < 240;i++){
	    newData[i] = i+2;
	}
	Pkt4o6Ptr pkt4o6_third(new Pkt4o6(newData,240,newData,240));
	Pkt4Ptr pkt4_third(new Pkt4(newData,240));
	
	//Create testing PKt4o6 now 
	Pkt4o6Ptr pkt4o6_third_(new Pkt4o6(pkt4o6_third,pkt4_third));
	Pkt4Ptr pkt4_third_ = pkt4o6_third_->getPkt4();
	Pkt6Ptr pkt6_third_ = pkt4o6_third_->getPkt6();
	pkt4_third_->repack();
	pkt6_third_->repack();
	data = (uint8_t*)pkt4_third_->getBuffer().getData();
	for(int i = 0;i < 10;i++)
	    EXPECT_EQ(i+2,data[i]);
	data = (uint8_t*)pkt6_third_->getBuffer().getData();
	for(int i = 0;i < 10;i++)
	    EXPECT_EQ(i+2,data[i]);
}

//test setJson and getJson 
TEST_F(Pkt4o6Test, jsontest) {

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
	
	//store pkt4o6 josn info.
	std::string json = pkt4o6->getJson();
	std::cout<<json<<std::endl;
	Pkt4o6Ptr pkt4o6_(new Pkt4o6(data,250,testData,250));
	pkt4o6_->setJson(json);
	int RemotePort = 546; 
	std::string RemoteAddr("fe80::21e:8cff:fe9b:7349");
	int LocalPort = 0;
    std::string LocalAddr("ff02::1:2");
    int Index = 2;
	std::string Iface("eth0");
	
	// test current pkt4o6 json info.
	Pkt6Ptr v6(pkt4o6_->getPkt6());
	EXPECT_EQ(RemotePort,v6->getRemotePort());
	EXPECT_EQ(RemoteAddr,v6->getRemoteAddr().toText());
	EXPECT_EQ(LocalPort,v6->getLocalPort());
	EXPECT_EQ(LocalAddr,v6->getLocalAddr().toText());
	EXPECT_EQ(Index,v6->getIndex());
	EXPECT_EQ(Iface,v6->getIface());
}

//test DHCPv4MsgOption
TEST_F(Pkt4o6Test, DHCPv4MsgOption) {
    uint8_t data[] = { 0, 1, 2, 3, 4, 5 };
    Pkt6Ptr pktv6(new Pkt6(data, sizeof(data)));
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
	
	//get DHCPv4MsgOption from Pkt4o6
	OptionBuffer buf(pkt4o6->getDHCPv4MsgOption());
	EXPECT_EQ(0,buf[0]);
	EXPECT_EQ(1,buf[1]);
	EXPECT_EQ(2,buf[2]);
	EXPECT_EQ(3,buf[3]);
	EXPECT_EQ(4,buf[4]);

}

} // end of anonymous namespace
