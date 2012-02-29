// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

#include <dns/message.h>
#include <dns/rcode.h>
#include <dns/rdataclass.h>

#include <datasrc/client.h>

#include <auth/query.h>

#include <algorithm>            // for std::max
#include <vector>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>

using namespace std;
using namespace isc::dns;
using namespace isc::datasrc;
using namespace isc::dns::rdata;

// Commonly used helper callback object for vector<ConstRRsetPtr> to
// insert them to (the specified section of) a message.
namespace {
class RRsetInserter {
public:
    RRsetInserter(Message& msg, Message::Section section, bool dnssec) :
        msg_(msg), section_(section), dnssec_(dnssec)
    {}
    void operator()(const ConstRRsetPtr& rrset) {
        msg_.addRRset(section_,
                      boost::const_pointer_cast<AbstractRRset>(rrset),
                      dnssec_);
    }

private:
    Message& msg_;
    const Message::Section section_;
    const bool dnssec_;
};
}

namespace isc {
namespace datasrc {
class FindContext {
public:
    FindContext(ZoneFinder& finder_param,
                const Name& qname_param,
                RRType qtype_param,
                const ZoneFinder::FindResult& result_param,
                ConstRRsetPtr rrset_param, bool dnssec_param) :
        code(result_param.code), rrset(rrset_param),
        finder(finder_param), qname(qname_param), qtype(qtype_param),
        dnssec(dnssec_param), nsec_signed(result_param.isNSECSigned()),
        nsec3_signed(result_param.isNSEC3Signed()),
        wildcard(result_param.isWildcard())
    {}

    FindContext(ZoneFinder& finder_param,
                const Name& qname_param,
                RRType qtype_param,
                const ZoneFinder::FindResult& result_param,
                ConstRRsetPtr rrset_param,
                vector<ConstRRsetPtr>& rrsets_param, bool dnssec_param) :
        code(result_param.code), rrset(rrset_param), rrsets(rrsets_param),
        finder(finder_param), qname(qname_param), qtype(qtype_param),
        dnssec(dnssec_param), nsec_signed(result_param.isNSECSigned()),
        nsec3_signed(result_param.isNSEC3Signed()),
        wildcard(result_param.isWildcard())
    {}
    const ZoneFinder::Result code;
    const ConstRRsetPtr rrset;
    vector<ConstRRsetPtr> rrsets; // note: this can't be a ref; we need a copy.

    // Called when adding 'additional' RRsets based on the context status.
    // Normally it adds A/AAA corresponding to the RRset identified in the
    // associated find() call.  When it was type ANY query, it returns
    // additional RRsets for each RRset in the returned vector.  If
    // getAtOrigin(RRType::NS()) has been called, it returns additional RRsets
    // for the result of getAtOrigin() (this is not really clean, hopefully
    // there's cleaner way).
    //
    // Default version: iterate over all RDATAs of the RRset(s) and call
    //   find(A/AAAA) for the corresponding part of the RDATA (NS name for NS,
    //   etc).
    // Optimized in-memory version: it (will) have shortcuts from the RRset
    //   to the rbt nodes of the corresonding additional RRsets.  It search
    //   for A/AAAA RRsets in the references nodes and returns them.
    void getAdditional(const vector<RRType>& requested_types,
                       vector<ConstRRsetPtr>& result);

    // Called for NXDOMAIN/NXRRSET cases and return NSEC/NSEC3 RRsets
    // for the corresponding DNSSEC proof.  It (will) consist of small helper
    // private methods for specific cases (NSEC or NSEC3, NXDOMAIN or NXRRSET).
    //
    // - NSEC + NXRRSET
    //   find() already returns the direct proof.  So it should already
    //   be efficient.  The ramining issue is the corresponding wildcard
    //   proof.
    //   Default version: calls find() for the qname again, with the
    //     NO_WILDCARD option and add the returned NSEC.
    //   Optimized in-memory version: it should know where in the rbtree the
    //     original (non wildcard) search stopped and can use the information
    //     to get the NSEC proof for the non existence of the qname itself.
    // - NSEC + NXDOMAIN
    //   find() already returns the NSEC the non existent proof, so it should
    //   be already efficient.  The ramining issue is the proof that there's
    //   even no wildcard match.
    //   Default version: construct the best possible wildcard name from qname
    //     and the NSEC that proves non-existent of the name, and internally
    //     calls find(NSEC) for that name.
    //   Optimized in-memory version: it should know the closest encloser in
    //     the previous search and can identify the wildcard name and its
    //     previous name, then subsequently its NSEC.
    //
    // - NSEC3 + NXDOMAIN
    //   Default version: use findNSEC3() to get closest encloser proof, use
    //     the result to identify the possible wildcard, and use findNSEC3()
    //     to get proof for its non-existence.
    //   Optimized in-memory version: it should know the closest encloser in
    //     the original search.  get corresponding NSEC3 directly, construct
    //     next closer and wildcard from the closest encloser, calculate hash
    //     and get the covering NSEC3.
    // - NSEC3 + NXRRSET (no wildcard)
    //   Default version: use findNSEC3() to get either closest encloser proof
    //     (optout DS case) or matching NSEC3.
    //   Optimized in-memory version: it knows the corresponding rbtnode
    //     for the qanem.  Get the hash value from it and get its NSEC3.
    //     If it doesn't exist and qtype was DS, search the tree back toward
    //     the root until it finds a matching NSEC3.
    // - NSEC3 + NXRRSET (wildcard)
    //   Default version: use findNSEC3() to get the closest encloser proof,
    //     construct the wildcard name based on the result, and get the NSEC3
    //     for it.
    //   Optimized in-memory version: similar to the NSEC3 + NXDOMAIN case.
    //     But the wildcard name should exist so it could even more optimize
    //     it by directly getting the wildcard node and its NSEC3.
    void getNegativeProof(vector<ConstRRsetPtr>& proofs);

    // Called for SUCCESS/CNAME cases when it's wildcard substitution
    // and return NSEC/NSEC3 RRset that proves the original qname doesn't
    // exist in the zone.
    // - With NSEC
    //     Default version: calls find() for the qname again, with the
    //       NO_WILDCARD option and add the returned NSEC.
    //     Optimized in-memory version: same for the NSEC + NXRRSET case above.
    // - With NSEC3
    //     Default version: call findNSEC3() in the recursive mode and returns
    //       the NSEC3 covering the next closer name.
    //     Optimized in-memory version: Like the NSEC case it should know the
    //       closest encloser in the original search and easily construct the
    //       next closer name from it.  Then calculate hash for it and search
    //       for the covering NSEC3.
    void getWildcardProof(vector<ConstRRsetPtr>& proofs);

    // Called for DELEGATION case.  Return either DS (if it's signed
    // delegation), NSEC/NSEC3 (if unsigned delegation and the parent is
    // signed with NSEC/NSEC3).
    //
    // Default versiopn: internally calls find() for the delegation name/DS.
    //   On SUCCESS return the answer RRset; On NXRRSET + NSEC (and if NSEC
    //   returned), return the returned (NSEC) RRset; On NXRRSET + NSEC3,
    //   call findNSEC3() recursive mode for the delegation name, and return
    //   exact or closest encloser proof.
    // Optimized in-memory version: it should know the rbtree node for the
    //   delegation point.  If it has DS, return it; if not but it has NSEC,
    //   return it; otherwise same as NSEC3 + NXRRSET (no wild) case for
    //   getNegativeProof().
    void getDelegationProof(vector<ConstRRsetPtr>& proofs);

    // Called for DNAME case.  (Somehow) construct the synthesized CNAME
    // for the qname with the DNAME.  Return Rcode of NOERROR() normally,
    // but YXDOMAIN() if CNAME cannot be constructed because the name would
    // be too long.
    //
    // Default version: extract the DNAME RDATA, construct the synthesized
    //   cname by splitting and concatinating labels, create a new standard
    //   CNAME RRset and sets its (only) RDATA to the created cname.
    // Optimized in-memory version: construct the synthesized cname
    //   essentially same way, but possibly more efficiently exploiting the
    //   internal representation of the in-memory RDATA (without involving
    //   expensive name splitting and concatinating).  To avoid resource
    //   allocation it might use some pool of "free (and empty)" internal
    //   RRset.
    Rcode getSynthesizedCNAME(vector<ConstRRsetPtr>& proofs);

    // Called to get origin NS or SOA (although the method is generalized).
    // Default version: internally calls find() for the origin name and the
    //   given type, and return the resul.t
    // Optimized in-memory version: it has a shortcut to the origin node.
    //   get directly the requested RRset there and return it.
    ConstRRsetPtr getAtOrigin(RRType type);

private:
    void getAdditionalForRRset(const AbstractRRset& base_rrset,
                               const vector<RRType>& requested_types,
                               vector<ConstRRsetPtr>& result);
    void getAdditionalAddrs(const Name& name,
                            const vector<RRType>& requested_types,
                            vector<ConstRRsetPtr>& result,
                            ZoneFinder::FindOptions options);

    uint8_t getClosestEncloserProof(const Name& name,
                                    vector<ConstRRsetPtr>& proofs,
                                    bool exact_ok, bool add_closest = true);
    ConstRRsetPtr getNSEC3ForName(const Name& name, bool match);
    void getNXDOMAINProofByNSEC(vector<ConstRRsetPtr>& proofs);
    ConstRRsetPtr getNoWildcardNSEC();
    void getWildcardNXRRSETProof(vector<ConstRRsetPtr>& proofs);

private:
    ZoneFinder& finder;
    const Name& qname;
    const RRType qtype;
    const bool dnssec;
    const bool nsec_signed;
    const bool nsec3_signed;
    const bool wildcard;
    ConstRRsetPtr ns_rrset;
};

Rcode
FindContext::getSynthesizedCNAME(vector<ConstRRsetPtr>& records) {
    /*
     * Empty DNAME should never get in, as it is impossible to
     * create one in master file.
     *
     * FIXME: Other way to prevent this should be done
     */
    assert(rrset->getRdataCount() > 0);
    // Get the data of DNAME
    const rdata::generic::DNAME& dname(
        dynamic_cast<const rdata::generic::DNAME&>(
            rrset->getRdataIterator()->getCurrent()));
    // The yet unmatched prefix dname
    const Name prefix(qname.split(0, qname.getLabelCount() -
                                  rrset->getName().getLabelCount()));
    // If we put it together, will it be too long?
    // (The prefix contains trailing ., which will be removed
    if (prefix.getLength() - Name::ROOT_NAME().getLength() +
        dname.getDname().getLength() > Name::MAX_WIRE) {
        /*
         * In case the synthesized name is too long, section 4.1
         * of RFC 2672 mandates we return YXDOMAIN.
         */
        return (Rcode::YXDOMAIN());
    }
    // The new CNAME we are creating (it will be unsigned even
    // with DNSSEC, the DNAME is signed and it can be validated
    // by that)
    RRsetPtr cname(new RRset(qname, rrset->getClass(),
                             RRType::CNAME(), rrset->getTTL()));

    // Construct the new target by replacing the end
    cname->addRdata(generic::CNAME(qname.split(0,
                                               qname.getLabelCount() -
                                               rrset->getName().
                                               getLabelCount()).
                                   concatenate(dname.getDname())));
    records.push_back(cname);
    return (Rcode::NOERROR());
}

ConstRRsetPtr
FindContext::getAtOrigin(RRType type) {
    const ZoneFinder::FindOptions options = dnssec ?
        ZoneFinder::FIND_DNSSEC : ZoneFinder::FIND_DEFAULT;
    ZoneFinder::FindResult result = finder.find(finder.getOrigin(), type,
                                                options);
    if (result.code != ZoneFinder::SUCCESS) {
        if (type == RRType::SOA()) {
            isc_throw(auth::Query::NoSOA,
                      "There's no given record in zone origin "
                      << finder.getOrigin().toText());
        } else if (type == RRType::NS()) {
            isc_throw(auth::Query::NoApexNS,
                      "There's no given record in zone origin "
                      << finder.getOrigin().toText());
        } else {
            assert(false);      // for simplicity
        }
    }
    if (type == RRType::NS()) {
        ns_rrset = result.rrset;
    }
    return (result.rrset);
}

void
FindContext::getAdditional(const vector<RRType>& requested_types,
                           vector<ConstRRsetPtr>& result)
{
    if (rrset || ns_rrset) {
        getAdditionalForRRset(ns_rrset ? *ns_rrset : *rrset, requested_types,
                              result);
    } else {
        BOOST_FOREACH(ConstRRsetPtr rrset_in_set, rrsets) {
            getAdditionalForRRset(*rrset_in_set, requested_types, result);
        }
    }
}

void
FindContext::getAdditionalForRRset(const AbstractRRset& base_rrset,
                                   const vector<RRType>& requested_types,
                                   vector<ConstRRsetPtr>& result)
{
    RdataIteratorPtr rdata_iterator(base_rrset.getRdataIterator());
    ZoneFinder::FindOptions options = ZoneFinder::FIND_DEFAULT;
    if (dnssec) {
        options = options | ZoneFinder::FIND_DNSSEC;
    }
    for (; !rdata_iterator->isLast(); rdata_iterator->next()) {
        const Rdata& rdata(rdata_iterator->getCurrent());

        if (base_rrset.getType() == RRType::NS()) {
            // Need to perform the search in the "GLUE OK" mode.
            const generic::NS& ns = dynamic_cast<const generic::NS&>(rdata);
            getAdditionalAddrs(ns.getNSName(), requested_types, result,
                               options | ZoneFinder::FIND_GLUE_OK);
        } else if (base_rrset.getType() == RRType::MX()) {
            const generic::MX& mx(dynamic_cast<const generic::MX&>(rdata));
            getAdditionalAddrs(mx.getMXName(), requested_types, result,
                               options);
        }
    }
}

void
FindContext::getAdditionalAddrs(const Name& name,
                                const vector<RRType>& requested_types,
                                vector<ConstRRsetPtr>& result_rrsets,
                                ZoneFinder::FindOptions options)
{
    // Out of zone name
    const NameComparisonResult cmp = finder.getOrigin().compare(name);
    if ((cmp.getRelation() != NameComparisonResult::SUPERDOMAIN) &&
        (cmp.getRelation() != NameComparisonResult::EQUAL)) {
        return;
    }

    vector<RRType>::const_iterator it = requested_types.begin();
    vector<RRType>::const_iterator it_end = requested_types.end();
    for (; it != it_end; ++it) {
        ZoneFinder::FindResult result = finder.find(name, *it, options);
        if (result.code == ZoneFinder::SUCCESS) {
            result_rrsets.push_back(result.rrset);
        }
    }
}

ConstRRsetPtr
FindContext::getNoWildcardNSEC() {
    // Next, identify the best possible wildcard name that would match
    // the query name.  It's the longer common suffix with the qname
    // between the owner or the next domain of the NSEC that proves NXDOMAIN,
    // prefixed by the wildcard label, "*".  For example, for query name
    // a.b.example.com, if the NXDOMAIN NSEC is
    // b.example.com. NSEC c.example.com., the longer suffix is b.example.com.,
    // and the best possible wildcard is *.b.example.com.  If the NXDOMAIN
    // NSEC is a.example.com. NSEC c.b.example.com., the longer suffix
    // is the next domain of the NSEC, and we get the same wildcard name.
    const int qlabels = qname.getLabelCount();
    const int olabels = qname.compare(rrset->getName()).getCommonLabels();
    const int nlabels = qname.compare(
        dynamic_cast<const generic::NSEC&>(rrset->getRdataIterator()->
                                           getCurrent()).
        getNextName()).getCommonLabels();
    const int common_labels = std::max(olabels, nlabels);
    const Name wildname(Name("*").concatenate(qname.split(qlabels -
                                                          common_labels)));

    // Confirm the wildcard doesn't exist (this should result in NXDOMAIN;
    // otherwise we shouldn't have got NXDOMAIN for the original query in
    // the first place).
    const ZoneFinder::FindResult fresult =
        finder.find(wildname, RRType::NSEC(), ZoneFinder::FIND_DNSSEC);
    if (fresult.code != ZoneFinder::NXDOMAIN || !fresult.rrset ||
        fresult.rrset->getRdataCount() == 0) {
        return (ConstRRsetPtr());
    }

    return (fresult.rrset);
}

void
FindContext::getNegativeProof(vector<ConstRRsetPtr>& proofs) {
    switch (code) {
    case ZoneFinder::NXDOMAIN: { // should eventually go to a method
        if (nsec_signed) {
            getNXDOMAINProofByNSEC(proofs);
            return;
        }
        // Firstly get the NSEC3 proves for Closest Encloser Proof
        // See Section 7.2.1 of RFC 5155.
        const uint8_t closest_labels =
            getClosestEncloserProof(qname, proofs, false);
        // Next, construct the wildcard name at the closest encloser, i.e.,
        // '*' followed by the closest encloser, and get NSEC3 for it.
        const Name wildname(Name("*").concatenate(
                                qname.split(qname.getLabelCount() -
                                             closest_labels)));
        proofs.push_back(getNSEC3ForName(wildname, false));
    }
        break;
    case ZoneFinder::NXRRSET:
        if (nsec_signed && rrset) {
            proofs.push_back(rrset);
        } else if (nsec3_signed && !wildcard) {
            if (qtype == RRType::DS()) {
                // RFC 5155, Section 7.2.4.  Add either NSEC3 for the qname or
                // closest (provable) encloser proof in case of optout.
                getClosestEncloserProof(qname, proofs, true);
            } else {
                // RFC 5155, Section 7.2.3.  Just add NSEC3 for the qname.
                proofs.push_back(getNSEC3ForName(qname, true));
            }
        }
        if (wildcard) {
            getWildcardNXRRSETProof(proofs);
        }
        break;
    default:
        assert(false);
    }
}

void
FindContext::getNXDOMAINProofByNSEC(vector<ConstRRsetPtr>& proofs) {
    if (!rrset) {
        return;
    }
    if (rrset->getRdataCount() == 0) {
        isc_throw(auth::Query::BadNSEC, "NSEC for NXDOMAIN is empty");
    }
    proofs.push_back(rrset);
    ConstRRsetPtr wnsec = getNoWildcardNSEC();
    if (!wnsec) {
        isc_throw(auth::Query::BadNSEC,
                  "Unexpected result for wildcard NXDOMAIN proof");
    }
    // Add the (no-) wildcard proof only when it's different from the NSEC
    // that proves NXDOMAIN; sometimes they can be the same.
    // Note: name comparison is relatively expensive.  When we are at the
    // stage of performance optimization, we should consider optimizing this
    // for some optimized data source implementations.
    if (rrset->getName() != wnsec->getName()) {
        proofs.push_back(wnsec);
    }
}


void
FindContext::getWildcardProof(vector<ConstRRsetPtr>& proofs) {
    if (nsec_signed) {
        // Case for RFC4035 Section 3.1.3.3.
        //
        // The query name shouldn't exist in the zone if there were no wildcard
        // substitution.  Confirm that by specifying NO_WILDCARD.  It should
        // result in NXDOMAIN and an NSEC RR that proves it should be returned.
        const ZoneFinder::FindResult fresult =
            finder.find(qname, RRType::NSEC(),
                        ZoneFinder::FIND_DNSSEC | ZoneFinder::NO_WILDCARD);
        if (fresult.code != ZoneFinder::NXDOMAIN || !fresult.rrset ||
            fresult.rrset->getRdataCount() == 0) {
            isc_throw(isc::auth::Query::BadNSEC,
                      "Unexpected NSEC result for wildcard proof");
        }
        proofs.push_back(fresult.rrset);
    } else if (nsec3_signed) {
        // Case for RFC 5155 Section 7.2.6.
        //
        // Note that the closest encloser must be the immediate ancestor
        // of the matching wildcard, so NSEC3 for its next closer (and only
        // that NSEC3) is what we are expected to provided per the RFC (if
        // this assumption isn't met the zone is broken anyway).
        getClosestEncloserProof(qname, proofs, false, false);
    }
}

void
FindContext::getDelegationProof(vector<ConstRRsetPtr>& proofs) {
    ZoneFinder::FindResult ds_result =
        finder.find(rrset->getName(), RRType::DS(), ZoneFinder::FIND_DNSSEC);
    FindContext ds_ctx(finder, rrset->getName(), RRType::DS(),
                       ds_result, ds_result.rrset, dnssec);
    if (ds_result.code == ZoneFinder::SUCCESS) {
        proofs.push_back(ds_result.rrset);
    } else if (ds_result.code == ZoneFinder::NXRRSET &&
               ds_result.isNSECSigned() && ds_result.rrset) {
        proofs.push_back(ds_result.rrset);
    } else if (ds_result.code == ZoneFinder::NXRRSET &&
               ds_result.isNSEC3Signed()) {
        // Add no DS proof with NSEC3 as specified in RFC 5155 Section 7.2.7.
        getClosestEncloserProof(rrset->getName(), proofs, true);
    } else {
        // Any other case should be an error
        isc_throw(auth::Query::BadDS,
                  "Unexpected result for DS lookup for delegation");
    }
}

void
FindContext::getWildcardNXRRSETProof(vector<ConstRRsetPtr>& proofs) {
    if (nsec_signed) {
        // There should be one NSEC RR which was found in the zone to prove
        // that there is not matched <QNAME,QTYPE> via wildcard expansion.
        if (rrset->getRdataCount() == 0) {
            isc_throw(auth::Query::BadNSEC,
                      "NSEC for WILDCARD_NXRRSET is empty");
        }
    
        const ZoneFinder::FindResult fresult =
            finder.find(qname, RRType::NSEC(),
                        ZoneFinder::FIND_DNSSEC | ZoneFinder::NO_WILDCARD);
        if (fresult.code != ZoneFinder::NXDOMAIN || !fresult.rrset ||
            fresult.rrset->getRdataCount() == 0) {
            isc_throw(auth::Query::BadNSEC,
                      "Unexpected result for no match QNAME proof");
        }

        if (rrset->getName() != fresult.rrset->getName()) {
            // one NSEC RR proves wildcard_nxrrset that no matched QNAME.
            proofs.push_back(fresult.rrset);
        }
    } else if (nsec3_signed) {
        // Case for RFC 5155 Section 7.2.5: add closest encloser proof for the
        // qname, construct the matched wildcard name and add NSEC3 for it.
        const uint8_t closest_labels =
            getClosestEncloserProof(qname, proofs, false);
        const Name wname = Name("*").concatenate(
            qname.split(qname.getLabelCount() - closest_labels));
        proofs.push_back(getNSEC3ForName(wname, true));
    }
}

uint8_t
FindContext::getClosestEncloserProof(const Name& name,
                                     vector<ConstRRsetPtr>& proofs,
                                     bool exact_ok, bool add_closest)
{
    const ZoneFinder::FindNSEC3Result result = finder.findNSEC3(name, true);

    // Validity check (see the method description).  Note that a completely
    // broken findNSEC3 implementation could even return NULL RRset in
    // closest_proof.  We don't explicitly check such case; addRRset() will
    // throw an exception, and it will be converted to SERVFAIL at the caller.
    if (!exact_ok && !result.next_proof) {
        isc_throw(isc::auth::Query::BadNSEC3,
                  "Matching NSEC3 found for a non existent name: " << name);
    }

    if (add_closest) {
        proofs.push_back(result.closest_proof);
    }
    if (result.next_proof) {
        proofs.push_back(result.next_proof);
    }
    return (result.closest_labels);
}

ConstRRsetPtr
FindContext::getNSEC3ForName(const Name& name, bool match) {
    const ZoneFinder::FindNSEC3Result result = finder.findNSEC3(name, false);

    // See the comment for getClosestEncloserProof().  We don't check a
    // totally bogus case where closest_proof is NULL here.
    if (match != result.matched) {
        isc_throw(isc::auth::Query::BadNSEC3, "Unexpected "
                  << (result.matched ? "matching" : "covering")
                  << " NSEC3 found for " << name);
    }
    return (result.closest_proof);
}

} // namespace datasrc
} // namespace isc

namespace isc {
namespace auth {

void
getAdditional(const Name& qname, RRType qtype, FindContext& ctx,
              vector<ConstRRsetPtr>& results)
{
    vector<RRType> needed_types;
    needed_types.push_back(RRType::A());
    needed_types.push_back(RRType::AAAA());

    vector<ConstRRsetPtr> additionals;
    ctx.getAdditional(needed_types, additionals);

    vector<ConstRRsetPtr>::const_iterator it = additionals.begin();
    vector<ConstRRsetPtr>::const_iterator it_end = additionals.end();
    for (; it != it_end; ++it) {
        if ((qtype == (*it)->getType() || qtype == RRType::ANY()) &&
            qname == (*it)->getName()) {
            continue;
        }
        results.push_back(*it);
    }
}

namespace {
// A simple wrapper for DataSourceClient::findZone().  Normally we can simply
// check the closest zone to the qname, but for type DS query we need to
// look into the parent zone.  Nevertheless, if there is no "parent" (i.e.,
// the qname consists of a single label, which also means it's the root name),
// we should search the deepest zone we have (which should be the root zone;
// otherwise it's a query error).
DataSourceClient::FindResult
findZone(const DataSourceClient& client, const Name& qname, RRType qtype) {
    if (qtype != RRType::DS() || qname.getLabelCount() == 1) {
        return (client.findZone(qname));
    }
    return (client.findZone(qname.split(1)));
}
}

void
Query::process() {
    // Found a zone which is the nearest ancestor to QNAME
    const DataSourceClient::FindResult result = findZone(datasrc_client_,
                                                         qname_, qtype_);

    // If we have no matching authoritative zone for the query name, return
    // REFUSED.  In short, this is to be compatible with BIND 9, but the
    // background discussion is not that simple.  See the relevant topic
    // at the BIND 10 developers's ML:
    // https://lists.isc.org/mailman/htdig/bind10-dev/2010-December/001633.html
    if (result.code != result::SUCCESS &&
        result.code != result::PARTIALMATCH) {
        // If we tried to find a "parent zone" for a DS query and failed,
        // we may still have authority at the child side.  If we do, the query
        // has to be handled there.
        if (qtype_ == RRType::DS() && qname_.getLabelCount() > 1 &&
            processDSAtChild()) {
            return;
        }
        response_.setHeaderFlag(Message::HEADERFLAG_AA, false);
        response_.setRcode(Rcode::REFUSED());
        return;
    }
    ZoneFinder& zfinder = *result.zone_finder;

    // We have authority for a zone that contain the query name (possibly
    // indirectly via delegation).  Look into the zone.
    response_.setHeaderFlag(Message::HEADERFLAG_AA);
    response_.setRcode(Rcode::NOERROR());
    boost::function0<ZoneFinder::FindResult> find;
    vector<ConstRRsetPtr> answers;
    vector<ConstRRsetPtr> authorities;
    vector<ConstRRsetPtr> additionals;
    const bool qtype_is_any = (qtype_ == RRType::ANY());
    if (qtype_is_any) {
        find = boost::bind(&ZoneFinder::findAll, &zfinder, qname_,
                           boost::ref(answers), dnssec_opt_);
    } else {
        find = boost::bind(&ZoneFinder::find, &zfinder, qname_, qtype_,
                           dnssec_opt_);
    }
    ZoneFinder::FindResult db_result(find());
    FindContext ctx = qtype_is_any ?
        FindContext(zfinder, qname_, qtype_, db_result, db_result.rrset,
                    answers, dnssec_) :
        FindContext(zfinder, qname_, qtype_, db_result, db_result.rrset,
                    dnssec_);

    switch (db_result.code) {
        case ZoneFinder::DNAME: {
            // First, put the dname into the answer
            answers.push_back(db_result.rrset);
            const Rcode rcode = ctx.getSynthesizedCNAME(answers);
            if (rcode != Rcode::NOERROR()) {
                response_.setRcode(rcode);
            }
            break;
        }
        case ZoneFinder::CNAME:
            /*
             * We don't do chaining yet. Therefore handling a CNAME is
             * mostly the same as handling SUCCESS, but we didn't get
             * what we expected. It means no exceptions in ANY or NS
             * on the origin (though CNAME in origin is probably
             * forbidden anyway).
             *
             * So, just put it there.
             */
            answers.push_back(db_result.rrset);

            // If the answer is a result of wildcard substitution,
            // add a proof that there's no closer name.
            if (dnssec_ && db_result.isWildcard()) {
                ctx.getWildcardProof(authorities);
            }
            break;
        case ZoneFinder::SUCCESS:
            if (!qtype_is_any) {
                answers.push_back(db_result.rrset);
            }
            // Handle additional for answer section
            getAdditional(qname_, qtype_, ctx, additionals);

            // If apex NS records haven't been provided in the answer
            // section, insert apex NS records into the authority section
            // and AAAA/A RRS of each of the NS RDATA into the additional
            // section.
            if (qname_ != result.zone_finder->getOrigin() ||
                db_result.code != ZoneFinder::SUCCESS ||
                (qtype_ != RRType::NS() && !qtype_is_any))
            {
                authorities.push_back(ctx.getAtOrigin(RRType::NS()));
                getAdditional(qname_, qtype_, ctx, additionals);
            }

            // If the answer is a result of wildcard substitution,
            // add a proof that there's no closer name.
            if (dnssec_ && db_result.isWildcard()) {
                ctx.getWildcardProof(authorities);
            }
            break;
        case ZoneFinder::DELEGATION:
            // If a DS query resulted in delegation, we also need to check
            // if we are an authority of the child, too.  If so, we need to
            // complete the process in the child as specified in Section
            // 2.2.1.2. of RFC3658.
            if (qtype_ == RRType::DS() && processDSAtChild()) {
                return;
            }

            response_.setHeaderFlag(Message::HEADERFLAG_AA, false);
            authorities.push_back(db_result.rrset);
            getAdditional(qname_, qtype_, ctx, additionals);
            // If DNSSEC is requested, see whether there is a DS
            // record for this delegation.
            if (dnssec_) {
                ctx.getDelegationProof(authorities);
            }
            break;
        case ZoneFinder::NXDOMAIN:
            response_.setRcode(Rcode::NXDOMAIN());
            // Fall-through
        case ZoneFinder::NXRRSET:
            authorities.push_back(ctx.getAtOrigin(RRType::SOA()));
            if (dnssec_) {
                ctx.getNegativeProof(authorities);
            }
            break;
        default:
            // This is basically a bug of the data source implementation,
            // but could also happen in the middle of development where
            // we try to add a new result code.
            isc_throw(isc::NotImplemented, "Unknown result code");
            break;
    }

    for_each(answers.begin(), answers.end(),
             RRsetInserter(response_, Message::SECTION_ANSWER, dnssec_));
    for_each(authorities.begin(), authorities.end(),
             RRsetInserter(response_, Message::SECTION_AUTHORITY, dnssec_));
    for_each(additionals.begin(), additionals.end(),
             RRsetInserter(response_, Message::SECTION_ADDITIONAL,
                           dnssec_));
}

bool
Query::processDSAtChild() {
    const DataSourceClient::FindResult zresult =
        datasrc_client_.findZone(qname_);

    if (zresult.code != result::SUCCESS) {
        return (false);
    }

    // We are receiving a DS query at the child side of the owner name,
    // where the DS isn't supposed to belong.  We should return a "no data"
    // response as described in Section 3.1.4.1 of RFC4035 and Section
    // 2.2.1.1 of RFC 3658.  find(DS) should result in NXRRSET, in which
    // case (and if DNSSEC is required) we also add the proof for that,
    // but even if find() returns an unexpected result, we don't bother.
    // The important point in this case is to return SOA so that the resolver
    // that happens to contact us can hunt for the appropriate parent zone
    // by seeing the SOA.
    vector<ConstRRsetPtr> authorities;

    response_.setHeaderFlag(Message::HEADERFLAG_AA);
    response_.setRcode(Rcode::NOERROR());
    const ZoneFinder::FindResult ds_result =
        zresult.zone_finder->find(qname_, RRType::DS(), dnssec_opt_);
    FindContext ctx(*zresult.zone_finder, qname_, RRType::DS(),
                    ds_result, ds_result.rrset, dnssec_);
    authorities.push_back(ctx.getAtOrigin(RRType::SOA()));
    if (ds_result.code == ZoneFinder::NXRRSET && dnssec_) {
        ctx.getNegativeProof(authorities);
    }
    for_each(authorities.begin(), authorities.end(),
             RRsetInserter(response_, Message::SECTION_AUTHORITY, dnssec_));

    return (true);
}

}
}
