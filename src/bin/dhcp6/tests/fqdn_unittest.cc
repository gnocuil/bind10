// Copyright (C) 2013-2014  Internet Systems Consortium, Inc. ("ISC")
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
#include <dhcp_ddns/ncr_msg.h>
#include <dhcp/dhcp6.h>
#include <dhcp/option.h>
#include <dhcp/option_custom.h>
#include <dhcp/option6_client_fqdn.h>
#include <dhcp/option6_ia.h>
#include <dhcp/option6_iaaddr.h>
#include <dhcp/option_int_array.h>
#include <dhcpsrv/lease.h>

#include <dhcp6/tests/dhcp6_test_utils.h>
#include <boost/pointer_cast.hpp>
#include <gtest/gtest.h>

using namespace isc;
using namespace isc::test;
using namespace isc::asiolink;
using namespace isc::dhcp;
using namespace isc::dhcp_ddns;
using namespace isc::util;
using namespace isc::hooks;
using namespace std;

namespace {

/// @brief A test fixture class for testing DHCPv6 Client FQDN Option handling.
class FqdnDhcpv6SrvTest : public Dhcpv6SrvTest {
public:

    /// @brief Constructor
    FqdnDhcpv6SrvTest()
        : Dhcpv6SrvTest() {
        // generateClientId assigns DUID to duid_.
        generateClientId();
        lease_.reset(new Lease6(Lease::TYPE_NA, IOAddress("2001:db8:1::1"),
                                duid_, 1234, 501, 502, 503,
                                504, 1, 0));

    }

    /// @brief Destructor
    virtual ~FqdnDhcpv6SrvTest() {
    }

    /// @brief Construct the DHCPv6 Client FQDN option using flags and
    /// domain-name.
    ///
    /// @param flags Flags to be set for the created option.
    /// @param fqdn_name A name which should be stored in the option.
    /// @param fqdn_type A type of the name carried by the option: partial
    /// or fully qualified.
    ///
    /// @return A pointer to the created option.
    Option6ClientFqdnPtr
    createClientFqdn(const uint8_t flags,
                     const std::string& fqdn_name,
                     const Option6ClientFqdn::DomainNameType fqdn_type) {
        return (Option6ClientFqdnPtr(new Option6ClientFqdn(flags,
                                                           fqdn_name,
                                                           fqdn_type)));
    }

    /// @brief Create an instance of the lease, being used by tests.
    ///
    /// @param addr IPv6 address to be assigned to a lease.
    /// @param hostname Hostname to be associated with a lease.
    /// @param fqdn_fwd Boolean value which indicates whether forward DNS
    /// update has been performed for the lease.
    /// @param fqdn_rev Boolean value which indicates whether reverse DNS
    /// update has been performed for the lease.
    ///
    /// @return Instance of the lease being created.
    Lease6Ptr createLease(const isc::asiolink::IOAddress& addr,
                          const std::string& hostname,
                          const bool fqdn_fwd,
                          const bool fqdn_rev) {
        Lease6Ptr lease(new Lease6(Lease::TYPE_NA, addr, duid_, 1, 200, 300,
                                   60, 90, 1, fqdn_fwd, fqdn_rev,
                                   hostname));
        return (lease);
    }

    /// @brief Create a message with or without DHCPv6 Client FQDN Option.
    ///
    /// @param msg_type A type of the DHCPv6 message to be created.
    /// @param fqdn_flags Flags to be carried in the FQDN option.
    /// @param fqdn_domain_name A name to be carried in the FQDN option.
    /// @param fqdn_type A type of the name carried by the option: partial
    /// or fully qualified.
    /// @param include_oro A boolean value which indicates whether the ORO
    /// option should be added to the message (if true).
    /// @param srvid server id to be stored in the message.
    ///
    /// @return An object representing the created message.
    Pkt6Ptr generateMessage(uint8_t msg_type,
                            const uint8_t fqdn_flags,
                            const std::string& fqdn_domain_name,
                            const Option6ClientFqdn::DomainNameType
                            fqdn_type,
                            const bool include_oro,
                            OptionPtr srvid = OptionPtr()) {
        Pkt6Ptr pkt = Pkt6Ptr(new Pkt6(msg_type, 1234));
        pkt->setRemoteAddr(IOAddress("fe80::abcd"));
        Option6IAPtr ia = generateIA(D6O_IA_NA, 234, 1500, 3000);

        if (msg_type != DHCPV6_REPLY) {
            IOAddress hint("2001:db8:1:1::dead:beef");
            OptionPtr hint_opt(new Option6IAAddr(D6O_IAADDR, hint, 300, 500));
            ia->addOption(hint_opt);
            pkt->addOption(ia);
        }

        OptionPtr clientid = generateClientId();
        pkt->addOption(clientid);
        if (srvid && (msg_type != DHCPV6_SOLICIT)) {
            pkt->addOption(srvid);
        }

        pkt->addOption(createClientFqdn(fqdn_flags, fqdn_domain_name,
                                            fqdn_type));

        if (include_oro) {
            OptionUint16ArrayPtr oro(new OptionUint16Array(Option::V6,
                                                           D6O_ORO));
            oro->addValue(D6O_CLIENT_FQDN);
            pkt->addOption(oro);
        }

        return (pkt);
    }

    /// @brief Creates instance of the DHCPv6 message with client id and
    /// server id.
    ///
    /// @param msg_type A type of the message to be created.
    /// @param srv An object representing the DHCPv6 server, which
    /// is used to generate the client identifier.
    ///
    /// @return An object representing the created message.
    Pkt6Ptr generateMessageWithIds(const uint8_t msg_type,
                                   NakedDhcpv6Srv& srv) {
        Pkt6Ptr pkt = Pkt6Ptr(new Pkt6(msg_type, 1234));
        // Generate client-id.
        OptionPtr opt_clientid = generateClientId();
        pkt->addOption(opt_clientid);

        if (msg_type != DHCPV6_SOLICIT) {
            // Generate server-id.
            pkt->addOption(srv.getServerID());
        }

        return (pkt);
    }

    /// @brief Returns an instance of the option carrying FQDN.
    ///
    /// @param pkt A message holding FQDN option to be returned.
    ///
    /// @return An object representing DHCPv6 Client FQDN option.
    Option6ClientFqdnPtr getClientFqdnOption(const Pkt6Ptr& pkt) {
        return (boost::dynamic_pointer_cast<Option6ClientFqdn>
                (pkt->getOption(D6O_CLIENT_FQDN)));
    }

    /// @brief Adds IA option to the message.
    ///
    /// Addded option holds an address.
    ///
    /// @param iaid IAID
    /// @param pkt A DHCPv6 message to which the IA option should be added.
    void addIA(const uint32_t iaid, const IOAddress& addr, Pkt6Ptr& pkt) {
        Option6IAPtr opt_ia = generateIA(D6O_IA_NA, iaid, 1500, 3000);
        Option6IAAddrPtr opt_iaaddr(new Option6IAAddr(D6O_IAADDR, addr,
                                                      300, 500));
        opt_ia->addOption(opt_iaaddr);
        pkt->addOption(opt_ia);
    }


    /// @brief Adds IA option to the message.
    ///
    /// Added option holds status code.
    ///
    /// @param iaid IAID
    /// @param status_code Status code
    /// @param pkt A DHCPv6 message to which the option should be added.
    void addIA(const uint32_t iaid, const uint16_t status_code, Pkt6Ptr& pkt) {
        Option6IAPtr opt_ia = generateIA(D6O_IA_NA, iaid, 1500, 3000);
        addStatusCode(status_code, "", opt_ia);
        pkt->addOption(opt_ia);
    }

    /// @brief Creates status code with the specified code and message.
    ///
    /// @param code A status code.
    /// @param msg A string representation of the message to be added to the
    /// Status Code option.
    ///
    /// @return An object representing the Status Code option.
    OptionCustomPtr createStatusCode(const uint16_t code,
                                     const std::string& msg) {
        OptionDefinition def("status-code", D6O_STATUS_CODE, "record");
        def.addRecordField("uint16");
        def.addRecordField("string");
        OptionCustomPtr opt_status(new OptionCustom(def, Option::V6));
        opt_status->writeInteger(code);
        if (!msg.empty()) {
            opt_status->writeString(msg, 1);
        }
        return (opt_status);
    }

    /// @brief Adds Status Code option to the IA.
    ///
    /// @param code A status code value.
    /// @param msg A string representation of the message to be added to the
    /// Status Code option.
    void addStatusCode(const uint16_t code, const std::string& msg,
                       Option6IAPtr& opt_ia) {
        opt_ia->addOption(createStatusCode(code, msg));
    }

    /// @brief Verifies if the DHCPv6 server processes DHCPv6 Client FQDN option
    /// as expected.
    ///
    /// This function simulates generation of the client's message holding FQDN.
    /// It then calls the server's @c Dhcpv6Srv::processClientFqdn option to
    /// generate server's response to the FQDN. This function returns the FQDN
    /// which should be appended to the server's response to the client.
    /// This function verifies that the FQDN option returned is correct.
    ///
    /// @param msg_type A type of the client's message.
    /// @param in_flags A value of flags field to be set for the FQDN carried
    /// in the client's message.
    /// @param in_domain_name A domain name to be carried in the client's FQDN
    /// option.
    /// @param in_domain_type A type of the domain name to be carried in the
    /// client's FQDM option (partial or fully qualified).
    /// @param exp_flags A value of flags expected in the FQDN sent by a server.
    /// @param exp_domain_name A domain name expected in the FQDN sent by a
    /// server.
    void testFqdn(const uint16_t msg_type,
                  const uint8_t in_flags,
                  const std::string& in_domain_name,
                  const Option6ClientFqdn::DomainNameType in_domain_type,
                  const uint8_t exp_flags,
                  const std::string& exp_domain_name) {
        NakedDhcpv6Srv srv(0);
        Pkt6Ptr question = generateMessage(msg_type,
                                           in_flags,
                                           in_domain_name,
                                           in_domain_type,
                                           true);
        ASSERT_TRUE(getClientFqdnOption(question));

        Pkt6Ptr answer(new Pkt6(msg_type == DHCPV6_SOLICIT ? DHCPV6_ADVERTISE :
                                DHCPV6_REPLY, question->getTransid()));
        ASSERT_NO_THROW(srv.processClientFqdn(question, answer));
        Option6ClientFqdnPtr answ_fqdn = boost::dynamic_pointer_cast<
            Option6ClientFqdn>(answer->getOption(D6O_CLIENT_FQDN));
        ASSERT_TRUE(answ_fqdn);

        const bool flag_n = (exp_flags & Option6ClientFqdn::FLAG_N) != 0;
        const bool flag_s = (exp_flags & Option6ClientFqdn::FLAG_S) != 0;
        const bool flag_o = (exp_flags & Option6ClientFqdn::FLAG_O) != 0;

        EXPECT_EQ(flag_n, answ_fqdn->getFlag(Option6ClientFqdn::FLAG_N));
        EXPECT_EQ(flag_s, answ_fqdn->getFlag(Option6ClientFqdn::FLAG_S));
        EXPECT_EQ(flag_o, answ_fqdn->getFlag(Option6ClientFqdn::FLAG_O));

        EXPECT_EQ(exp_domain_name, answ_fqdn->getDomainName());
        EXPECT_EQ(Option6ClientFqdn::FULL, answ_fqdn->getDomainNameType());
    }

    /// @brief Tests that the client's message holding an FQDN is processed
    /// and that lease is acquired.
    ///
    /// @param msg_type A type of the client's message.
    /// @param hostname A domain name in the client's FQDN.
    /// @param srv A server object, used to process the message.
    /// @param include_oro A boolean value which indicates whether the ORO
    /// option should be included in the client's message (if true) or not
    /// (if false). In the former case, the function will expect that server
    /// responds with the FQDN option. In the latter case, the function expects
    /// that the server doesn't respond with the FQDN.
    void testProcessMessage(const uint8_t msg_type,
                            const std::string& hostname,
                            NakedDhcpv6Srv& srv,
                            const bool include_oro = true) {
        // Create a message of a specified type, add server id and
        // FQDN option.
        OptionPtr srvid = srv.getServerID();
        Pkt6Ptr req = generateMessage(msg_type, Option6ClientFqdn::FLAG_S,
                                      hostname,
                                      Option6ClientFqdn::FULL,
                                      include_oro, srvid);

        // For different client's message types we have to invoke different
        // functions to generate response.
        Pkt6Ptr reply;
        if (msg_type == DHCPV6_SOLICIT) {
            ASSERT_NO_THROW(reply = srv.processSolicit(req));

        } else if (msg_type == DHCPV6_REQUEST) {
            ASSERT_NO_THROW(reply = srv.processRequest(req));

        } else if (msg_type == DHCPV6_RENEW) {
            ASSERT_NO_THROW(reply = srv.processRequest(req));

        } else if (msg_type == DHCPV6_RELEASE) {
            // For Release no lease will be acquired so we have to leave
            // function here.
            ASSERT_NO_THROW(reply = srv.processRelease(req));
            return;
        } else {
            // We are not interested in testing other message types.
            return;
        }

        // For Solicit, we will get different message type obviously.
        if (msg_type == DHCPV6_SOLICIT) {
            checkResponse(reply, DHCPV6_ADVERTISE, 1234);

        } else {
            checkResponse(reply, DHCPV6_REPLY, 1234);
        }

        // Check verify that IA_NA is correct.
        Option6IAAddrPtr addr =
            checkIA_NA(reply, 234, subnet_->getT1(), subnet_->getT2());
        ASSERT_TRUE(addr);

        // Check that we have got the address we requested.
        checkIAAddr(addr, IOAddress("2001:db8:1:1::dead:beef"),
                    Lease::TYPE_NA);

        if (msg_type != DHCPV6_SOLICIT) {
            // Check that the lease exists.
            Lease6Ptr lease =
                checkLease(duid_, reply->getOption(D6O_IA_NA), addr);
            ASSERT_TRUE(lease);
        }

        // The Client FQDN option should be always present in the server's
        // response, regardless if requested using ORO or not.
        ASSERT_TRUE(reply->getOption(D6O_CLIENT_FQDN));
    }

    /// @brief Verify that NameChangeRequest holds valid values.
    ///
    /// This function picks first NameChangeRequest from the internal server's
    /// queue and checks that it holds valid parameters. The NameChangeRequest
    /// is removed from the queue.
    ///
    /// @param srv A server object holding a queue of NameChangeRequests.
    /// @param type An expected type of the NameChangeRequest (Add or Remove).
    /// @param reverse An expected setting of the reverse update flag.
    /// @param forward An expected setting of the forward udpate flag.
    /// @param addr A string representation of the IPv6 address held in the
    /// NameChangeRequest.
    /// @param dhcid An expected DHCID value.
    /// @param cltt Timestamp when the lease has been acquired. The sum of
    /// this value and the len is compared with the lease expiration time
    /// held in NCR.
    /// @param len A valid lifetime of the lease associated with the
    /// NameChangeRequest.
    /// @param not_strict_expire_check Boolean value which indicates whether
    /// cltt + valid lifetime value should be checked for equality with a
    /// lease expiration time in the NameChangeRequest (if false) or
    /// it should be checked that the cltt + valid lifetime is greater or
    /// equal lease expiration time.
    void verifyNameChangeRequest(NakedDhcpv6Srv& srv,
                                 const isc::dhcp_ddns::NameChangeType type,
                                 const bool reverse, const bool forward,
                                 const std::string& addr,
                                 const std::string& dhcid,
                                 const time_t cltt,
                                 const uint16_t len,
                                 const bool not_strict_expire_check = false) {
        NameChangeRequest ncr = srv.name_change_reqs_.front();
        EXPECT_EQ(type, ncr.getChangeType());
        EXPECT_EQ(forward, ncr.isForwardChange());
        EXPECT_EQ(reverse, ncr.isReverseChange());
        EXPECT_EQ(addr, ncr.getIpAddress());
        EXPECT_EQ(dhcid, ncr.getDhcid().toStr());

        // In some cases, the test doesn't have access to the last transmission
        // time for the particular client. In such cases, the test can use the
        // current time as cltt but the it may not check the lease expiration
        // time for equality but rather check that the lease expiration time
        // is not greater than the current time + lease lifetime.
        if (not_strict_expire_check) {
            EXPECT_GE(cltt + len, ncr.getLeaseExpiresOn());
        } else {
            EXPECT_EQ(cltt + len, ncr.getLeaseExpiresOn());
        }

        EXPECT_EQ(len, ncr.getLeaseLength());
        EXPECT_EQ(isc::dhcp_ddns::ST_NEW, ncr.getStatus());
        srv.name_change_reqs_.pop();
    }

    // Holds a lease used by a test.
    Lease6Ptr lease_;

};

// A set of tests verifying server's behaviour when it receives the DHCPv6
// Client Fqdn Option.
// @todo: Extend these tests once appropriate configuration parameters are
// implemented (ticket #3034).

// Test server's response when client requests that server performs AAAA update.
TEST_F(FqdnDhcpv6SrvTest, serverAAAAUpdate) {
    testFqdn(DHCPV6_SOLICIT, Option6ClientFqdn::FLAG_S,
             "myhost.example.com",
             Option6ClientFqdn::FULL, Option6ClientFqdn::FLAG_S,
             "myhost.example.com.");
}

// Test server's response when client provides partial domain-name and requests
// that server performs AAAA update.
TEST_F(FqdnDhcpv6SrvTest, serverAAAAUpdatePartialName) {
    testFqdn(DHCPV6_SOLICIT, Option6ClientFqdn::FLAG_S, "myhost",
             Option6ClientFqdn::PARTIAL, Option6ClientFqdn::FLAG_S,
             "myhost.example.com.");
}

// Test server's response when client provides empty domain-name and requests
// that server performs AAAA update.
TEST_F(FqdnDhcpv6SrvTest, serverAAAAUpdateNoName) {
    testFqdn(DHCPV6_SOLICIT, Option6ClientFqdn::FLAG_S, "",
             Option6ClientFqdn::PARTIAL, Option6ClientFqdn::FLAG_S,
             "myhost.example.com.");
}

// Test server's response when client requests no DNS update.
TEST_F(FqdnDhcpv6SrvTest, noUpdate) {
    testFqdn(DHCPV6_SOLICIT, Option6ClientFqdn::FLAG_N,
             "myhost.example.com",
             Option6ClientFqdn::FULL, Option6ClientFqdn::FLAG_N,
             "myhost.example.com.");
}

// Test server's response when client requests that server delegates the AAAA
// update to the client and this delegation is not allowed.
TEST_F(FqdnDhcpv6SrvTest, clientAAAAUpdateNotAllowed) {
    testFqdn(DHCPV6_SOLICIT, 0, "myhost.example.com.",
             Option6ClientFqdn::FULL,
             Option6ClientFqdn::FLAG_S | Option6ClientFqdn::FLAG_O,
             "myhost.example.com.");
}

// Test that exactly one NameChangeRequest is generated when the new lease
// has been acquired (old lease is NULL).
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsNewLease) {
    NakedDhcpv6Srv srv(0);
    // Create a new lease.
    Lease6Ptr lease = createLease(IOAddress("2001:db8:1::3"),
        "myhost.example.com", true, true);
    // Old lease is NULL.
    Lease6Ptr old_lease;
    // When old lease is NULL, there should be one new NameChangeRequest
    // created which adds DNS entry for a new lease.
    ASSERT_NO_THROW(srv.createNameChangeRequests(lease, old_lease));
    ASSERT_EQ(1, srv.name_change_reqs_.size());

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1::3",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            lease->cltt_, 300);
}

// Test that no NameChangeRequest is generated when a lease is renewed and
// the FQDN data hasn't changed.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsRenewNoChange) {
    NakedDhcpv6Srv srv(0);

    // Create new lease.
    Lease6Ptr lease = createLease(IOAddress("2001:db8:1::3"),
                                  "myhost.example.com", true, true);
    // Create old lease, with the same FQDN data.
    Lease6Ptr old_lease = createLease(IOAddress("2001:db8:1::3"),
                                      "myhost.example.com", true, true);
    // The new lease renews the old lease, so the cltt should be updated
    // for the new lease.
    lease->cltt_ += old_lease->cltt_ + 10;

    ASSERT_NO_THROW(srv.createNameChangeRequests(lease, old_lease));
    // The FQDN data hasn't changed so, there should be no NCRs generated.
    EXPECT_TRUE(srv.name_change_reqs_.empty());
}

// Test that DNS entry is removed when forward and reverse flags are not
// set in the lease renewing an old lease.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsNoUpdate) {
    NakedDhcpv6Srv srv(0);
    Lease6Ptr lease1 = createLease(IOAddress("2001:db8:1::3"),
                                   "myhost.example.com", true, true);

    Lease6Ptr lease2 = createLease(IOAddress("2001:db8:1::3"),
                                   "myhost.example.com", false, false);

    // The new lease renews the old lease, so the cltt should be updated
    // for the new lease.
    lease2->cltt_ += lease1->cltt_ + 10;

    ASSERT_NO_THROW(srv.createNameChangeRequests(lease2, lease1));
    // There should be exactly one NCR generated. It removes the existing
    // DNS entries for the old lease.
    ASSERT_EQ(1, srv.name_change_reqs_.size());

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, true,
                            "2001:db8:1::3",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            lease1->cltt_, 300);

    // Do the same test, but this time, set the hostname to NULL.
    lease2->hostname_ = "";
    lease2->fqdn_rev_ = true;
    lease2->fqdn_fwd_ = true;
    ASSERT_NO_THROW(srv.createNameChangeRequests(lease2, lease1));
    EXPECT_EQ(1, srv.name_change_reqs_.size());

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, true,
                            "2001:db8:1::3",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            lease1->cltt_, 300);

}

// Test that two NameChangeRequests are generated when the lease is being
// renewed and the new lease has updated FQDN data.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsRenew) {
    NakedDhcpv6Srv srv(0);
    // Create two leases for the same IP address but with a different FQDNs.
    Lease6Ptr lease1 = createLease(IOAddress("2001:db8:1::3"),
                                   "lease1.example.com", true, true);

    Lease6Ptr lease2 = createLease(IOAddress("2001:db8:1::3"),
                                   "lease2.example.com", true, true);

    // The new lease renews the old lease, so the cltt should be updated
    // for the new lease.
    lease2->cltt_ += lease1->cltt_ + 10;

    // The FQDN is updated by the second lease, so there should be two
    // NCRs generated: one to remove the old FQDN, and another one to
    // add the updated FQDN.
    ASSERT_NO_THROW(srv.createNameChangeRequests(lease2, lease1));
    ASSERT_EQ(2, srv.name_change_reqs_.size());

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, true,
                            "2001:db8:1::3",
                            "0002015EDD017663C5AFAA6F33CB096A727CAF"
                            "0DD6BDC1A597D0AC5469AF4546204D3A",
                            lease1->cltt_, 300);

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1::3",
                            "00020133924373D25BD5C5A874976AD78BCF1BD"
                            "AC4D1D9084C2890E4800FC5E5F520E5",
                            lease2->cltt_, 300);
}

// This test verifies that exception is thrown when leases passed to the
// createNameChangeRequests function do not match, i.e. they comprise
// different IP addresses.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsLeaseMismatch) {
    NakedDhcpv6Srv srv(0);
    Lease6Ptr lease1 = createLease(IOAddress("2001:db8:1::3"),
                                   "lease1.example.com", true, true);

    Lease6Ptr lease2 = createLease(IOAddress("2001:db8:1::4"),
                                   "lease2.example.com", true, true);
    EXPECT_THROW(srv.createNameChangeRequests(lease2, lease1),
                 isc::Unexpected);
}

    /*// Test that exception is thrown if supplied NULL answer packet when
// creating NameChangeRequests.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsNoAnswer) {
    NakedDhcpv6Srv srv(0);

    Pkt6Ptr answer;

    EXPECT_THROW(srv.createNameChangeRequests(answer),
                 isc::Unexpected);

}

// Test that exception is thrown if supplied answer from the server
// contains no DUID when creating NameChangeRequests.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsNoDUID) {
    NakedDhcpv6Srv srv(0);

    Pkt6Ptr answer = Pkt6Ptr(new Pkt6(DHCPV6_REPLY, 1234));
    Option6ClientFqdnPtr fqdn = createClientFqdn(Option6ClientFqdn::FLAG_S,
                                                 "myhost.example.com",
                                                 Option6ClientFqdn::FULL);
    answer->addOption(fqdn);

    EXPECT_THROW(srv.createNameChangeRequests(answer), isc::Unexpected);

}

// Test no NameChangeRequests if Client FQDN is not added to the server's
// response.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsNoFQDN) {
    NakedDhcpv6Srv srv(0);

    // Create Reply message with Client Id and Server id.
    Pkt6Ptr answer = generateMessageWithIds(DHCPV6_REPLY, srv);

    ASSERT_NO_THROW(srv.createNameChangeRequests(answer));

    // There should be no new NameChangeRequests.
    EXPECT_TRUE(srv.name_change_reqs_.empty());
}

// Test that NameChangeRequests are not generated if an answer message
// contains no addresses.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequestsNoAddr) {
    NakedDhcpv6Srv srv(0);

    // Create Reply message with Client Id and Server id.
    Pkt6Ptr answer = generateMessageWithIds(DHCPV6_REPLY, srv);

    // Add Client FQDN option.
    Option6ClientFqdnPtr fqdn = createClientFqdn(Option6ClientFqdn::FLAG_S,
                                                 "myhost.example.com",
                                                 Option6ClientFqdn::FULL);
    answer->addOption(fqdn);

    ASSERT_NO_THROW(srv.createNameChangeRequests(answer));

    // We didn't add any IAs, so there should be no NameChangeRequests in th
    // queue.
    ASSERT_TRUE(srv.name_change_reqs_.empty());
}

// Test that a number of NameChangeRequests is created as a result of
// processing the answer message which holds 3 IAs and when FQDN is
// specified.
TEST_F(FqdnDhcpv6SrvTest, createNameChangeRequests) {
    NakedDhcpv6Srv srv(0);

    // Create Reply message with Client Id and Server id.
    Pkt6Ptr answer = generateMessageWithIds(DHCPV6_REPLY, srv);

    // Create three IAs, each having different address.
    addIA(1234, IOAddress("2001:db8:1::1"), answer);
    addIA(2345, IOAddress("2001:db8:1::2"), answer);
    addIA(3456, IOAddress("2001:db8:1::3"), answer);

    // Use domain name in upper case. It should be converted to lower-case
    // before DHCID is calculated. So, we should get the same result as if
    // we typed domain name in lower-case.
    Option6ClientFqdnPtr fqdn = createClientFqdn(Option6ClientFqdn::FLAG_S,
                                                 "MYHOST.EXAMPLE.COM",
                                                 Option6ClientFqdn::FULL);
    answer->addOption(fqdn);

    // Create NameChangeRequests. Since we have added 3 IAs, it should
    // result in generation of 3 distinct NameChangeRequests.
    ASSERT_NO_THROW(srv.createNameChangeRequests(answer));
    ASSERT_EQ(3, srv.name_change_reqs_.size());

    // Verify that NameChangeRequests are correct. Each call to the
    // verifyNameChangeRequest will pop verified request from the queue.

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1::1",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            0, 500);

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1::2",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            0, 500);

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1::3",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            0, 500);

}

// Test creation of the NameChangeRequest to remove both forward and reverse
// mapping for the given lease.
TEST_F(FqdnDhcpv6SrvTest, createRemovalNameChangeRequestFwdRev) {
    NakedDhcpv6Srv srv(0);

    lease_->fqdn_fwd_ = true;
    lease_->fqdn_rev_ = true;
    // Part of the domain name is in upper case, to test that it gets converted
    // to lower case before DHCID is computed. So, we should get the same DHCID
    // as if we typed domain-name in lower case.
    lease_->hostname_ = "MYHOST.example.com.";

    ASSERT_NO_THROW(srv.createRemovalNameChangeRequest(lease_));

    ASSERT_EQ(1, srv.name_change_reqs_.size());

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, true,
                            "2001:db8:1::1",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            0, 502);

}

// Test creation of the NameChangeRequest to remove reverse mapping for the
// given lease.
TEST_F(FqdnDhcpv6SrvTest, createRemovalNameChangeRequestRev) {
    NakedDhcpv6Srv srv(0);

    lease_->fqdn_fwd_ = false;
    lease_->fqdn_rev_ = true;
    lease_->hostname_ = "myhost.example.com.";

    ASSERT_NO_THROW(srv.createRemovalNameChangeRequest(lease_));

    ASSERT_EQ(1, srv.name_change_reqs_.size());

    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, false,
                            "2001:db8:1::1",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            0, 502);

}

// Test that NameChangeRequest to remove DNS records is not generated when
// neither forward nor reverse DNS update has been performed for a lease.
TEST_F(FqdnDhcpv6SrvTest, createRemovalNameChangeRequestNoUpdate) {
    NakedDhcpv6Srv srv(0);

    lease_->fqdn_fwd_ = false;
    lease_->fqdn_rev_ = false;

    ASSERT_NO_THROW(srv.createRemovalNameChangeRequest(lease_));

    EXPECT_TRUE(srv.name_change_reqs_.empty());

}

// Test that NameChangeRequest is not generated if the hostname hasn't been
// specified for a lease for which forward and reverse mapping has been set.
TEST_F(FqdnDhcpv6SrvTest, createRemovalNameChangeRequestNoHostname) {
    NakedDhcpv6Srv srv(0);

    lease_->fqdn_fwd_ = true;
    lease_->fqdn_rev_ = true;
    lease_->hostname_ = "";

    ASSERT_NO_THROW(srv.createRemovalNameChangeRequest(lease_));

    EXPECT_TRUE(srv.name_change_reqs_.empty());

}

// Test that NameChangeRequest is not generated if the invalid hostname has
// been specified for a lease for which forward and reverse mapping has been
// set.
TEST_F(FqdnDhcpv6SrvTest, createRemovalNameChangeRequestWrongHostname) {
    NakedDhcpv6Srv srv(0);

    lease_->fqdn_fwd_ = true;
    lease_->fqdn_rev_ = true;
    lease_->hostname_ = "myhost..example.com.";

    ASSERT_NO_THROW(srv.createRemovalNameChangeRequest(lease_));

    EXPECT_TRUE(srv.name_change_reqs_.empty());

    } */

// Test that Advertise message generated in a response to the Solicit will
// not result in generation if the NameChangeRequests.
TEST_F(FqdnDhcpv6SrvTest, processSolicit) {
    NakedDhcpv6Srv srv(0);

    // Create a Solicit message with FQDN option and generate server's
    // response using processSolicit function.
    testProcessMessage(DHCPV6_SOLICIT, "myhost.example.com", srv);
    EXPECT_TRUE(srv.name_change_reqs_.empty());
}

// Test that client may send two requests, each carrying FQDN option with
// a different domain-name. Server should use existing lease for the second
// request but modify the DNS entries for the lease according to the contents
// of the FQDN sent in the second request.
TEST_F(FqdnDhcpv6SrvTest, processTwoRequests) {
    NakedDhcpv6Srv srv(0);

    // Create a Request message with FQDN option and generate server's
    // response using processRequest function. This will result in the
    // creation of a new lease and the appropriate NameChangeRequest
    // to add both reverse and forward mapping to DNS.
    testProcessMessage(DHCPV6_REQUEST, "myhost.example.com", srv);
    ASSERT_EQ(1, srv.name_change_reqs_.size());
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            time(NULL), 4000, true);

    // Client may send another request message with a new domain-name. In this
    // case the same lease will be returned. The existing DNS entry needs to
    // be replaced with a new one. Server should determine that the different
    // FQDN has been already added to the DNS. As a result, the old DNS
    // entries should be removed and the entries for the new domain-name
    // should be added. Therefore, we expect two NameChangeRequests. One to
    // remove the existing entries, one to add new entries.
    testProcessMessage(DHCPV6_REQUEST, "otherhost.example.com", srv);
    ASSERT_EQ(2, srv.name_change_reqs_.size());
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            time(NULL), 4000, true);
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201D422AA463306223D269B6CB7AFE7AAD265FC"
                            "EA97F93623019B2E0D14E5323D5A",
                            time(NULL), 4000, true);

}

// Test that client may send Request followed by the Renew, both holding
// FQDN options, but each option holding different domain-name. The Renew
// should result in generation of the two NameChangeRequests, one to remove
// DNS entry added previously when Request was processed, another one to
// add a new entry for the FQDN held in the Renew.
TEST_F(FqdnDhcpv6SrvTest, processRequestRenew) {
    NakedDhcpv6Srv srv(0);

    // Create a Request message with FQDN option and generate server's
    // response using processRequest function. This will result in the
    // creation of a new lease and the appropriate NameChangeRequest
    // to add both reverse and forward mapping to DNS.
    testProcessMessage(DHCPV6_REQUEST, "myhost.example.com", srv);
    ASSERT_EQ(1, srv.name_change_reqs_.size());
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            time(NULL), 4000, true);

    // Client may send Renew message with a new domain-name. In this
    // case the same lease will be returned. The existing DNS entry needs to
    // be replaced with a new one. Server should determine that the different
    // FQDN has been already added to the DNS. As a result, the old DNS
    // entries should be removed and the entries for the new domain-name
    // should be added. Therefore, we expect two NameChangeRequests. One to
    // remove the existing entries, one to add new entries.
    testProcessMessage(DHCPV6_RENEW, "otherhost.example.com", srv);
    ASSERT_EQ(2, srv.name_change_reqs_.size());
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            time(NULL), 4000, true);
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201D422AA463306223D269B6CB7AFE7AAD265FC"
                            "EA97F93623019B2E0D14E5323D5A",
                            time(NULL), 4000, true);

}

TEST_F(FqdnDhcpv6SrvTest, processRequestRelease) {
    NakedDhcpv6Srv srv(0);

    // Create a Request message with FQDN option and generate server's
    // response using processRequest function. This will result in the
    // creation of a new lease and the appropriate NameChangeRequest
    // to add both reverse and forward mapping to DNS.
    testProcessMessage(DHCPV6_REQUEST, "myhost.example.com", srv);
    ASSERT_EQ(1, srv.name_change_reqs_.size());
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            time(NULL), 4000, true);

    // Client may send Release message. In this case the lease should be
    // removed and all existing DNS entries for this lease should be
    // also removed. Therefore, we expect that single NameChangeRequest to
    // remove DNS entries is generated.
    testProcessMessage(DHCPV6_RELEASE, "otherhost.example.com", srv);
    ASSERT_EQ(1, srv.name_change_reqs_.size());
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_REMOVE, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            time(NULL), 4000, true);

}

// Checks that the server include DHCPv6 Client FQDN option in its
// response even when client doesn't request this option using ORO.
TEST_F(FqdnDhcpv6SrvTest, processRequestWithoutFqdn) {
    NakedDhcpv6Srv srv(0);

    // The last parameter disables use of the ORO to request FQDN option
    // In this case, we expect that the FQDN option will not be included
    // in the server's response. The testProcessMessage will check that.
    testProcessMessage(DHCPV6_REQUEST, "myhost.example.com", srv, false);
    ASSERT_EQ(1, srv.name_change_reqs_.size());
    verifyNameChangeRequest(srv, isc::dhcp_ddns::CHG_ADD, true, true,
                            "2001:db8:1:1::dead:beef",
                            "000201415AA33D1187D148275136FA30300478"
                            "FAAAA3EBD29826B5C907B2C9268A6F52",
                            time(NULL), 4000, true);
}

}   // end of anonymous namespace
