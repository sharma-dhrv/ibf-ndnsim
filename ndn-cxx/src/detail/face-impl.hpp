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

#ifndef NDN_DETAIL_FACE_IMPL_HPP
#define NDN_DETAIL_FACE_IMPL_HPP

#include "../common.hpp"
#include "../face.hpp"

#include "registered-prefix.hpp"
#include "pending-interest.hpp"
#include "container-with-on-empty-signal.hpp"

#include "../util/scheduler.hpp"
#include "../util/config-file.hpp"
#include "../util/signal.hpp"

#include "../transport/transport.hpp"
#include "../transport/unix-transport.hpp"
#include "../transport/tcp-transport.hpp"

#include "../management/nfd-controller.hpp"
#include "../management/nfd-command-options.hpp"

#include <ns3/ptr.h>
#include <ns3/node.h>
#include <ns3/node-list.h>
#include <ns3/ndnSIM/model/ndn-l3-protocol.hpp>

#include "ns3/ndnSIM/NFD/daemon/face/local-face.hpp"

namespace ndn {

class Face::Impl : noncopyable
{
public:
  typedef ContainerWithOnEmptySignal<shared_ptr<PendingInterest>> PendingInterestTable;
  typedef std::list<shared_ptr<InterestFilterRecord> > InterestFilterTable;
  typedef ContainerWithOnEmptySignal<shared_ptr<RegisteredPrefix>> RegisteredPrefixTable;

  class NfdFace : public ::nfd::LocalFace
  {
  public:
    NfdFace(Impl& face, const ::nfd::FaceUri& localUri, const ::nfd::FaceUri& remoteUri)
      : ::nfd::LocalFace(localUri, remoteUri)
      , m_appFaceImpl(face)
    {
    }

  public: // from ::nfd::LocalFace
    /**
     * @brief Send Interest towards application
     */
    virtual void
    sendInterest(const Interest& interest)
    {
      NS_LOG_DEBUG("<< Interest " << interest);
      shared_ptr<const Interest> interestPtr = interest.shared_from_this();
      m_appFaceImpl.m_scheduler.scheduleEvent(time::seconds(0), [this, interestPtr] {
          m_appFaceImpl.processInterestFilters(*interestPtr);
        });
    }

    /**
     * @brief Send Data towards application
     */
    virtual void
    sendData(const Data& data)
    {
      NS_LOG_DEBUG("<< Data " << data.getName());
      shared_ptr<const Data> dataPtr = data.shared_from_this();
      m_appFaceImpl.m_scheduler.scheduleEvent(time::seconds(0), [this, dataPtr] {
          m_appFaceImpl.satisfyPendingInterests(*dataPtr);
        });
    }

    /** \brief Close the face
     *
     *  This terminates all communication on the face and cause
     *  onFail() method event to be invoked
     */
    virtual void
    close()
    {
      this->fail("close");
    }

  private:
    friend class Impl;
    Impl& m_appFaceImpl;
  };

  ////////////////////////////////////////////////////////////////////////

  explicit
  Impl(Face& face)
    : m_face(face)
    , m_scheduler(m_face.getIoService())
  {
    ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
    NS_ASSERT_MSG(node->GetObject<ns3::ndn::L3Protocol>() != 0,
                  "NDN stack should be installed on the node " << node);

    auto uri = ::nfd::FaceUri("ndnFace://" + boost::lexical_cast<std::string>(node->GetId()));
    m_nfdFace = make_shared<NfdFace>(*this, uri, uri);

    node->GetObject<ns3::ndn::L3Protocol>()->addFace(m_nfdFace);
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////

  void
  satisfyPendingInterests(const Data& data)
  {
    for (auto entry = m_pendingInterestTable.begin(); entry != m_pendingInterestTable.end(); ) {
      if ((*entry)->getInterest().matchesData(data)) {
        shared_ptr<PendingInterest> matchedEntry = *entry;

        entry = m_pendingInterestTable.erase(entry);

        matchedEntry->invokeDataCallback(data);
      }
      else
        ++entry;
    }
  }

  void
  processInterestFilters(const Interest& interest)
  {
    for (const auto& filter : m_interestFilterTable) {
      if (filter->doesMatch(interest.getName())) {
        filter->invokeInterestCallback(interest);
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////

  void
  asyncExpressInterest(const shared_ptr<const Interest>& interest,
                       const OnData& onData, const OnTimeout& onTimeout)
  {
    auto entry =
      m_pendingInterestTable.insert(make_shared<PendingInterest>(interest,
                                                                 onData, onTimeout,
                                                                 ref(m_scheduler))).first;
    (*entry)->setDeleter([this, entry] { m_pendingInterestTable.erase(entry); });

    m_nfdFace->emitSignal(onReceiveInterest, *interest);
  }

  void
  asyncRemovePendingInterest(const PendingInterestId* pendingInterestId)
  {
    m_pendingInterestTable.remove_if(MatchPendingInterestId(pendingInterestId));
  }

  void
  asyncPutData(const shared_ptr<const Data>& data)
  {
    m_nfdFace->emitSignal(onReceiveData, *data);
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////

  void
  asyncSetInterestFilter(const shared_ptr<InterestFilterRecord>& interestFilterRecord)
  {
    m_interestFilterTable.push_back(interestFilterRecord);
  }

  void
  asyncUnsetInterestFilter(const InterestFilterId* interestFilterId)
  {
    InterestFilterTable::iterator i = std::find_if(m_interestFilterTable.begin(),
                                                   m_interestFilterTable.end(),
                                                   MatchInterestFilterId(interestFilterId));
    if (i != m_interestFilterTable.end())
      {
        m_interestFilterTable.erase(i);
      }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////

  const RegisteredPrefixId*
  registerPrefix(const Name& prefix,
                 const shared_ptr<InterestFilterRecord>& filter,
                 const RegisterPrefixSuccessCallback& onSuccess,
                 const RegisterPrefixFailureCallback& onFailure,
                 uint64_t flags,
                 const nfd::CommandOptions& options)
  {
    using namespace nfd;

    ControlParameters params;
    params.setName(prefix);
    params.setFlags(flags);

    auto prefixToRegister = make_shared<RegisteredPrefix>(prefix, filter, options);

    m_face.m_nfdController->start<RibRegisterCommand>(params,
                                                      bind(&Impl::afterPrefixRegistered, this,
                                                           prefixToRegister, onSuccess),
                                                      bind(onFailure, prefixToRegister->getPrefix(), _2),
                                                      options);

    return reinterpret_cast<const RegisteredPrefixId*>(prefixToRegister.get());
  }

  void
  afterPrefixRegistered(const shared_ptr<RegisteredPrefix>& registeredPrefix,
                        const RegisterPrefixSuccessCallback& onSuccess)
  {
    m_registeredPrefixTable.insert(registeredPrefix);

    if (static_cast<bool>(registeredPrefix->getFilter())) {
      // it was a combined operation
      m_interestFilterTable.push_back(registeredPrefix->getFilter());
    }

    if (static_cast<bool>(onSuccess)) {
      onSuccess(registeredPrefix->getPrefix());
    }
  }

  void
  asyncUnregisterPrefix(const RegisteredPrefixId* registeredPrefixId,
                        const UnregisterPrefixSuccessCallback& onSuccess,
                        const UnregisterPrefixFailureCallback& onFailure)
  {
    using namespace nfd;
    auto i = std::find_if(m_registeredPrefixTable.begin(),
                          m_registeredPrefixTable.end(),
                          MatchRegisteredPrefixId(registeredPrefixId));
    if (i != m_registeredPrefixTable.end()) {
      RegisteredPrefix& record = **i;

      const shared_ptr<InterestFilterRecord>& filter = record.getFilter();

      if (filter != nullptr) {
        // it was a combined operation
        m_interestFilterTable.remove(filter);
      }

      ControlParameters params;
      params.setName(record.getPrefix());
      m_face.m_nfdController->start<RibUnregisterCommand>(params,
                                                          bind(&Impl::finalizeUnregisterPrefix, this, i, onSuccess),
                                                          bind(onFailure, _2),
                                                          record.getCommandOptions());
    }
    else {
      if (onFailure != nullptr) {
        onFailure("Unrecognized PrefixId");
      }
    }

    // there cannot be two registered prefixes with the same id
  }

  void
  finalizeUnregisterPrefix(RegisteredPrefixTable::iterator item,
                           const UnregisterPrefixSuccessCallback& onSuccess)
  {
    m_registeredPrefixTable.erase(item);

    if (static_cast<bool>(onSuccess)) {
      onSuccess();
    }
  }

private:
  Face& m_face;
  util::Scheduler m_scheduler;

  PendingInterestTable m_pendingInterestTable;
  InterestFilterTable m_interestFilterTable;
  RegisteredPrefixTable m_registeredPrefixTable;

  shared_ptr<NfdFace> m_nfdFace;

  friend class Face;
};

} // namespace ndn

#endif // NDN_DETAIL_FACE_IMPL_HPP
