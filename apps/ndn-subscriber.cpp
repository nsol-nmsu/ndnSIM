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

#include "ndn-subscriber.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "utils/ndn-ns3-packet-tag.hpp"
#include "model/ndn-app-face.hpp"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/ref.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.Subscriber");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(Subscriber);

TypeId
Subscriber::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::Subscriber")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<Subscriber>()

      .AddAttribute("StartSeq", "Initial sequence number", IntegerValue(0),
                    MakeIntegerAccessor(&Subscriber::m_seq), MakeIntegerChecker<int32_t>())
      .AddAttribute("Prefix", "Name of the Interest", StringValue("/"),
                    MakeNameAccessor(&Subscriber::m_interestName), MakeNameChecker())
      .AddAttribute("LifeTime", "LifeTime for subscription packet", StringValue("1200s"),
                    MakeTimeAccessor(&Subscriber::m_interestLifeTime), MakeTimeChecker())
      .AddAttribute("TxTimer",
                    "Timeout defining how frequently subscription should be reinforced",
		    TimeValue(Seconds(9)),
                    MakeTimeAccessor(&Subscriber::m_txInterval), MakeTimeChecker())
      ;

  return tid;
}

Subscriber::Subscriber()
    : m_rand(CreateObject<UniformRandomVariable>())
    , m_seq(0)
    , m_seqMax(std::numeric_limits<uint32_t>::max()) // set to max value on uint32
    , m_firstTime (true)

{
  NS_LOG_FUNCTION_NOARGS();

  m_rtt = CreateObject<RttMeanDeviation>();
}


Subscriber::~Subscriber()
{
}


void
Subscriber::ScheduleNextPacket()
{
  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &Subscriber::SendPacket, this);
    m_firstTime = false;
  }
  else if (!m_sendEvent.IsRunning()) {
    m_sendEvent = Simulator::Schedule(m_txInterval, &Subscriber::SendPacket, this);
  }
}

void
Subscriber::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();
  ScheduleNextPacket();
}

void
Subscriber::StopApplication() // Called at time specified by Stop
{
  NS_LOG_FUNCTION_NOARGS();
  Simulator::Cancel(m_sendEvent);
  App::StopApplication();
}

void
Subscriber::SendPacket()
{

  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  uint32_t seq = std::numeric_limits<uint32_t>::max();

  if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return;
      }
  }

  seq = m_seq++;

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  //nameWithSequence->appendSequenceNumber(seq);
  //

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setSubscription(1);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("node(" << GetNode()->GetId() << ") > Interest for " << interest->getName());

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_face->onReceiveInterest(*interest);

  ScheduleNextPacket();

}


///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
Subscriber::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") < Received DATA for " << data->getName());

  // NS_LOG_INFO ("Received content object: " << boost::cref(*data));


  
}

void
Subscriber::WillSendOutInterest(uint32_t sequenceNumber)
{
  NS_LOG_DEBUG("Trying to add " << sequenceNumber << " with " << Simulator::Now() << ". already "
                                << m_seqTimeouts.size() << " items");

  m_seqTimeouts.insert(SeqTimeout(sequenceNumber, Simulator::Now()));
  m_seqFullDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqLastDelay.erase(sequenceNumber);
  m_seqLastDelay.insert(SeqTimeout(sequenceNumber, Simulator::Now()));

  m_seqRetxCounts[sequenceNumber]++;

  m_rtt->SentSeq(SequenceNumber32(sequenceNumber), 1);

}


} // namespace ndn
} // namespace ns3
