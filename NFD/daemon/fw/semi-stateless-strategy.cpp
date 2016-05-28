/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "semi-stateless-strategy.hpp"
#include "core/logger.hpp"

#include <ndn-cxx/bloom-filter.hpp>

namespace nfd {
namespace fw {

NFD_LOG_INIT("SemiStatelessStrategy");

const Name SemiStatelessStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/semi-stateless-strategy/%FD%03");
NFD_REGISTER_STRATEGY(SemiStatelessStrategy);

SemiStatelessStrategy::SemiStatelessStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

static inline bool
predicate_NextHop_eligible(const shared_ptr<pit::Entry>& pitEntry,
  const fib::NextHop& nexthop, FaceId currentDownstream,
  bool wantUnused = false,
  time::steady_clock::TimePoint now = time::steady_clock::TimePoint::min())
{
  shared_ptr<Face> upstream = nexthop.getFace();

  BloomFilter lid = upstream->ibf;
  BloomFilter ibf = (pitEntry->getInterest()).ibf;

  if (ibf.matchLID(lid))
  {
    return true;
  }
  else
  {
    return false;
  }
}

void
SemiStatelessStrategy::afterReceiveInterest(const Face& inFace,
                                         const Interest& interest,
                                         shared_ptr<fib::Entry> fibEntry,
                                         shared_ptr<pit::Entry> pitEntry)
{
  const fib::NextHopList& nexthops = fibEntry->getNextHops();
  fib::NextHopList::const_iterator it = nexthops.end();

  it = std::find_if(nexthops.begin(), nexthops.end(),
                    bind(&predicate_NextHop_eligible, pitEntry, _1, inFace.getId(),
                         true, time::steady_clock::now()));
  if (it != nexthops.end()) {
    shared_ptr<Face> outFace = it->getFace();
    this->sendInterest(pitEntry, outFace);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " retransmit-unused-to=" << outFace->getId());
    return;
  } else {
    NFD_LOG_DEBUG("Semi-Stateless Strategy : No upstream face found");
  }

}

} // namespace fw
} // namespace nfd
