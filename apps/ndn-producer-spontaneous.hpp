/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NDN_PRODUCER_SPONTANEOUS_H
#define NDN_PRODUCER_SPONTANEOUS_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-app.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ns3/nstime.h"
#include "ns3/ptr.h"

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief A producer which publishes updates of its data at a fixed frequency.
 * These updates are sent regardless of whether an interest was received
 * or not and nodes which subscribe to the data will receive it. Installed
 * on nodes at the compute layer of Smart Grid architecture (iCenS)
 */
class SpontaneousProducer : public App {
public:
  static TypeId
  GetTypeId(void);

  /// Default constructor for the class
  SpontaneousProducer();

  // inherited from NdnApp
  virtual void
  OnInterest(shared_ptr<const Interest> interest);

  /**
   * @brief Send data to subscribed nodes (typically prosumers at the physical layer)
   */
  void
  SendData(const Name &dataName);

  void
  SendTimeout();

public:
  typedef void (*ReceivedInterestTraceCallback)( uint32_t, shared_ptr<const Interest> );
  typedef void (*SentDataTraceCallback)( uint32_t, shared_ptr<const Data> );

protected:
  // inherited from Application base class.
  virtual void
  StartApplication(); // Called at time specified by Start

  virtual void
  StopApplication(); // Called at time specified by Stop

private:
  Name m_prefix;
  Name m_postfix;
  uint32_t m_virtualPayloadSize;
  Time m_freshness;
  Time m_frequency;
  EventId m_txEvent;
  bool m_firstTime;
  uint32_t m_subscription;
  Name m_prefixWithoutSequence;
  size_t m_receivedpayload;
  size_t m_subDataSize; //Size of subscription data, in Kbytes
  
  uint32_t m_signature;
  Name m_keyLocator;

protected:
  TracedCallback <  uint32_t, shared_ptr<const Interest> > m_receivedInterest;
  TracedCallback <  uint32_t, shared_ptr<const Data> > m_sentData;

};

} // namespace ndn
} // namespace ns3

#endif // NDN_PRODUCER_H
