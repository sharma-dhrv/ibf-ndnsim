/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2015 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#include "face.hpp"

#include "ns3/log.h"
NS_LOG_COMPONENT_DEFINE("ndn-cxx.Face");

#include "detail/face-impl.hpp"

#include "encoding/tlv.hpp"
#include "security/key-chain.hpp"
#include "security/signing-helpers.hpp"
#include "util/time.hpp"
#include "util/random.hpp"
#include "util/face-uri.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"

namespace ndn {

Face::Face()
  : m_impl(new Impl(*this))
{
  construct();
}

Face::Face(boost::asio::io_service&)
  : m_impl(new Impl(*this))
{
  construct();
}

void
Face::construct()
{
  static ::ndn::KeyChain keyChain("pib-dummy", "tpm-dummy");

  m_nfdController.reset(new nfd::Controller(*this, ns3::ndn::StackHelper::getKeyChain()));
}

Face::~Face() = default;

const PendingInterestId*
Face::expressInterest(const Interest& interest, const OnData& onData, const OnTimeout& onTimeout)
{
  NS_LOG_INFO (">> Interest: " << interest.getName());

  shared_ptr<Interest> interestToExpress = make_shared<Interest>(interest);
  m_impl->m_scheduler.scheduleEvent(time::seconds(0), [=] {
      m_impl->asyncExpressInterest(interestToExpress, onData, onTimeout);
    });

  return reinterpret_cast<const PendingInterestId*>(interestToExpress.get());
}

const PendingInterestId*
Face::expressInterest(const Name& name,
                      const Interest& tmpl,
                      const OnData& onData, const OnTimeout& onTimeout/* = OnTimeout()*/)
{
  return expressInterest(Interest(tmpl)
                           .setName(name)
                           .setNonce(0),
                         onData, onTimeout);
}

void
Face::put(const Data& data)
{
  NS_LOG_INFO (">> Data: " << data.getName());

  shared_ptr<const Data> dataPtr;
  try {
    dataPtr = data.shared_from_this();
  }
  catch (const bad_weak_ptr& e) {
    NS_LOG_INFO("Face::put WARNING: the supplied Data should be created using make_shared<Data>()");
    dataPtr = make_shared<Data>(data);
  }

  m_impl->m_scheduler.scheduleEvent(time::seconds(0), [=] {
      m_impl->asyncPutData(dataPtr);
    });
}

void
Face::removePendingInterest(const PendingInterestId* pendingInterestId)
{
  m_impl->m_scheduler.scheduleEvent(time::seconds(0),
                                    [=] {
                                      m_impl->asyncRemovePendingInterest(pendingInterestId);
                                    });
}

size_t
Face::getNPendingInterests() const
{
  return m_impl->m_pendingInterestTable.size();
}

const RegisteredPrefixId*
Face::setInterestFilter(const InterestFilter& interestFilter,
                        const OnInterest& onInterest,
                        const RegisterPrefixFailureCallback& onFailure,
                        const security::SigningInfo& signingInfo,
                        uint64_t flags)
{
  return setInterestFilter(interestFilter,
                           onInterest,
                           RegisterPrefixSuccessCallback(),
                           onFailure,
                           signingInfo,
                           flags);
}

const RegisteredPrefixId*
Face::setInterestFilter(const InterestFilter& interestFilter,
                        const OnInterest& onInterest,
                        const RegisterPrefixSuccessCallback& onSuccess,
                        const RegisterPrefixFailureCallback& onFailure,
                        const security::SigningInfo& signingInfo,
                        uint64_t flags)
{
  shared_ptr<InterestFilterRecord> filter =
    make_shared<InterestFilterRecord>(interestFilter, onInterest);

  nfd::CommandOptions options;
  options.setSigningInfo(signingInfo);

  return m_impl->registerPrefix(interestFilter.getPrefix(), filter,
                                onSuccess, onFailure,
                                flags, options);
}

const InterestFilterId*
Face::setInterestFilter(const InterestFilter& interestFilter,
                        const OnInterest& onInterest)
{
  NS_LOG_INFO("Set Interest Filter << " << interestFilter);

  shared_ptr<InterestFilterRecord> filter =
    make_shared<InterestFilterRecord>(interestFilter, onInterest);

  m_impl->m_scheduler.scheduleEvent(time::seconds(0),
                                    [=] { m_impl->asyncSetInterestFilter(filter); });

  return reinterpret_cast<const InterestFilterId*>(filter.get());
}

#ifdef NDN_FACE_KEEP_DEPRECATED_REGISTRATION_SIGNING

const RegisteredPrefixId*
Face::setInterestFilter(const InterestFilter& interestFilter,
                        const OnInterest& onInterest,
                        const RegisterPrefixSuccessCallback& onSuccess,
                        const RegisterPrefixFailureCallback& onFailure,
                        const IdentityCertificate& certificate,
                        uint64_t flags)
{
  security::SigningInfo signingInfo;
  if (!certificate.getName().empty()) {
    signingInfo = signingByCertificate(certificate.getName());
  }
  return setInterestFilter(interestFilter, onInterest,
                           onSuccess, onFailure,
                           signingInfo, flags);
}

const RegisteredPrefixId*
Face::setInterestFilter(const InterestFilter& interestFilter,
                        const OnInterest& onInterest,
                        const RegisterPrefixFailureCallback& onFailure,
                        const IdentityCertificate& certificate,
                        uint64_t flags)
{
  security::SigningInfo signingInfo;
  if (!certificate.getName().empty()) {
    signingInfo = signingByCertificate(certificate.getName());
  }
  return setInterestFilter(interestFilter, onInterest,
                             onFailure, signingInfo, flags);
}

const RegisteredPrefixId*
Face::setInterestFilter(const InterestFilter& interestFilter,
                        const OnInterest& onInterest,
                        const RegisterPrefixSuccessCallback& onSuccess,
                        const RegisterPrefixFailureCallback& onFailure,
                        const Name& identity,
                        uint64_t flags)
{
  security::SigningInfo signingInfo = signingByIdentity(identity);

  return setInterestFilter(interestFilter, onInterest,
                           onSuccess, onFailure,
                           signingInfo, flags);
}

const RegisteredPrefixId*
Face::setInterestFilter(const InterestFilter& interestFilter,
                        const OnInterest& onInterest,
                        const RegisterPrefixFailureCallback& onFailure,
                        const Name& identity,
                        uint64_t flags)
{
  security::SigningInfo signingInfo = signingByIdentity(identity);

  return setInterestFilter(interestFilter, onInterest,
                           onFailure, signingInfo, flags);
}

#endif // NDN_FACE_KEEP_DEPRECATED_REGISTRATION_SIGNING

const RegisteredPrefixId*
Face::registerPrefix(const Name& prefix,
                     const RegisterPrefixSuccessCallback& onSuccess,
                     const RegisterPrefixFailureCallback& onFailure,
                     const security::SigningInfo& signingInfo,
                     uint64_t flags)
{

  nfd::CommandOptions options;
  options.setSigningInfo(signingInfo);

  return m_impl->registerPrefix(prefix, shared_ptr<InterestFilterRecord>(),
                                onSuccess, onFailure,
                                flags, options);
}

#ifdef NDN_FACE_KEEP_DEPRECATED_REGISTRATION_SIGNING

const RegisteredPrefixId*
Face::registerPrefix(const Name& prefix,
                     const RegisterPrefixSuccessCallback& onSuccess,
                     const RegisterPrefixFailureCallback& onFailure,
                     const IdentityCertificate& certificate,
                     uint64_t flags)
{
  security::SigningInfo signingInfo;
  if (!certificate.getName().empty()) {
    signingInfo = signingByCertificate(certificate.getName());
  }
  return registerPrefix(prefix, onSuccess,
                        onFailure, signingInfo, flags);
}

const RegisteredPrefixId*
Face::registerPrefix(const Name& prefix,
                     const RegisterPrefixSuccessCallback& onSuccess,
                     const RegisterPrefixFailureCallback& onFailure,
                     const Name& identity,
                     uint64_t flags)
{
  security::SigningInfo signingInfo = signingByIdentity(identity);
  return registerPrefix(prefix, onSuccess,
                        onFailure, signingInfo, flags);
}
#endif // NDN_FACE_KEEP_DEPRECATED_REGISTRATION_SIGNING

void
Face::unsetInterestFilter(const RegisteredPrefixId* registeredPrefixId)
{
  m_impl->m_scheduler.scheduleEvent(time::seconds(0), [=] {
      m_impl->asyncUnregisterPrefix(registeredPrefixId,
                                    UnregisterPrefixSuccessCallback(),
                                    UnregisterPrefixFailureCallback()); });
}

void
Face::unsetInterestFilter(const InterestFilterId* interestFilterId)
{
  m_impl->m_scheduler.scheduleEvent(time::seconds(0), [=] {
      m_impl->asyncUnsetInterestFilter(interestFilterId);
    });
}

void
Face::unregisterPrefix(const RegisteredPrefixId* registeredPrefixId,
                       const UnregisterPrefixSuccessCallback& onSuccess,
                       const UnregisterPrefixFailureCallback& onFailure)
{
  m_impl->m_scheduler.scheduleEvent(time::seconds(0), [=] {
      m_impl->asyncUnregisterPrefix(registeredPrefixId,onSuccess, onFailure);
    });
}

void
Face::processEvents(const time::milliseconds& timeout/* = time::milliseconds::zero()*/,
                    bool keepThread/* = false*/)
{
}

void
Face::shutdown()
{
  m_impl->m_scheduler.scheduleEvent(time::seconds(0), [=] {
      m_impl->m_pendingInterestTable.clear();
      m_impl->m_registeredPrefixTable.clear();

      m_impl->m_nfdFace->close();
    });
}

} // namespace ndn
