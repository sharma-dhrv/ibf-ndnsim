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

#include "common.hpp"

#include "scheduler.hpp"

namespace ns3 {

/// @cond include_hidden

template<>
struct EventMemberImplObjTraits<std::function<void()>> {
  typedef std::function<void()> T;
  static T&
  GetReference(T& p)
  {
    return p;
  }
};

/// @endcond

} // namespace ns3

namespace ndn {
namespace util {
namespace scheduler {

struct EventIdImpl
{
  EventIdImpl(const Scheduler::EventQueue::iterator& event)
    : m_event(event)
    , m_isValid(true)
  {
  }

  void
  invalidate()
  {
    m_isValid = false;
  }

  bool
  isValid() const
  {
    return m_isValid;
  }

  operator const Scheduler::EventQueue::iterator&() const
  {
    return m_event;
  }

  void
  reset(const Scheduler::EventQueue::iterator& newIterator)
  {
    m_event = newIterator;
    m_isValid = true;
  }

private:
  Scheduler::EventQueue::iterator m_event;
  bool m_isValid;
};

Scheduler::EventInfo::EventInfo(const time::nanoseconds& after,
                                const Event& event)
  : m_scheduledTime(time::steady_clock::now() + after)
  , m_event(event)
{
}

Scheduler::EventInfo::EventInfo(const time::steady_clock::TimePoint& when,
                                const EventInfo& previousEvent)
  : m_scheduledTime(when)
  , m_event(previousEvent.m_event)
  , m_eventId(previousEvent.m_eventId)
{
}

time::nanoseconds
Scheduler::EventInfo::expiresFromNow() const
{
  time::steady_clock::TimePoint now = time::steady_clock::now();
  if (now > m_scheduledTime)
    return time::seconds(0); // event should be scheduled ASAP
  else
    return m_scheduledTime - now;
}


Scheduler::Scheduler(boost::asio::io_service& ioService)
  : m_scheduledEvent(m_events.end())
{
}

EventId
Scheduler::scheduleEvent(const time::nanoseconds& after,
                         const Event& event)
{
  ns3::EventId id = ns3::Simulator::Schedule(ns3::NanoSeconds(after.count()),
                                             &std::function<void()>::operator(), event);
  auto id_ptr = std::make_shared<ns3::EventId>(id);
  m_events.insert(id_ptr);
  return id_ptr;
}

void
Scheduler::cancelEvent(const EventId& eventId)
{
  if (eventId != nullptr) {
    ns3::Simulator::Remove(*eventId);
    const_cast<EventId&>(eventId).reset();
    m_events.erase(eventId);
  }
}

void
Scheduler::cancelAllEvents()
{
  for (auto i = m_events.begin(); i != m_events.end(); i++) {
    if ((*i) != nullptr) {
      ns3::Simulator::Remove((**i));
      const_cast<EventId&>(*i).reset();
    }
  }
  m_events.clear();
}

} // namespace scheduler
} // namespace util
} // namespace ndn
