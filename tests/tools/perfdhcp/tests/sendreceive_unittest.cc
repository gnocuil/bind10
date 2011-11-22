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

#include <stdint.h>
#include <gtest/gtest.h>

#include "../cloptions.h"
#include "../netio.h"

TEST(SendReceiveTest, v4) {
    // Invoke command line processor to set up for IPv4 operation, localhost
    const char* argv[] = { "perfdhcp", "127.0.0.1" };
    const char message[] = "This is a test for IPv4";
    char buf[1024];
    int numOctets;

    EXPECT_EQ(1, procArgs(sizeof(argv)/sizeof(char *), argv));
    dhcpSetup("20942", NULL, "20942");
    EXPECT_EQ(1, dhcpSend((const unsigned char*)message, sizeof(message)));
    numOctets = dhcpReceive(buf, sizeof(buf));
    EXPECT_EQ(sizeof(message), numOctets);
    if (numOctets == sizeof(message)) {
        EXPECT_STREQ(message, buf);
    }
    netShutdown();
}

TEST(SendReceiveTest, v6) {
    // Invoke command line processor to set up for IPv6 operation, localhost
    const char* argv[] = { "perfdhcp", "-6", "::1" };
    const char message[] = "This is a test for IPv6";
    char buf[1024];
    int numOctets;

    EXPECT_EQ(1, procArgs(sizeof(argv)/sizeof(char *), argv));
    dhcpSetup("20942", NULL, "20942");
    EXPECT_EQ(1, dhcpSend((const unsigned char*)message, sizeof(message)));
    numOctets = dhcpReceive(buf, sizeof(buf));
    EXPECT_EQ(sizeof(message), numOctets);
    if (numOctets == sizeof(message)) {
        EXPECT_STREQ(message, buf);
    }
    netShutdown();
}
