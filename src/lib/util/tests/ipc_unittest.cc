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


#include <util/ipc.h>

#include <gtest/gtest.h>

using namespace isc::util;


namespace {


class IPCTest : public ::testing::Test {
    public:
        IPCTest()
        {

        }
};

// Test BaseIPC constructor
TEST_F(IPCTest, constructor) {
	BaseIPC ipc;
	EXPECT_EQ(-1, ipc.getSocket());
}


// Test openSocket function
TEST_F(IPCTest, openSocket) {
	BaseIPC ipc;
	int fd = ipc.openSocket();
	
	EXPECT_EQ(fd, ipc.getSocket());
}


// Test bindSocket function
TEST_F(IPCTest, send_recv) {
	BaseIPC local,remote;
	uint8_t data[] = { 1, 2, 3, 4, 5, 6 };
	local.openSocket();
	remote.openSocket();
	local.bindSocket("server");
	remote.setRemote("server");
	OutputBuffer sendmsg(6);
	for(int i = 0;i < 6;i++){
		sendmsg.writeUint8(data[i]);
	}
	remote.send(sendmsg);
	
	//now localIPC receive some data from remoteIPC
	InputBuffer recvmsg= local.recv();
	
	//check out length.
	ASSERT_EQ(recvmsg.getLength(), sendmsg.getLength());
	for(int i = 0;i < 6;i++){
		uint8_t num = recvmsg.readUint8();
		EXPECT_EQ(num, data[i]);
	}
}

}

