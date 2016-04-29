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

      .AddAttribute("RetxTimer",
                    "Timeout defining how frequent retransmission timeouts should be checked",
                    StringValue("50ms"),
                    MakeTimeAccessor(&Subscriber::GetRetxTimer, &Subscriber::SetRetxTimer),
                    MakeTimeChecker())

      .AddAttribute("Subscription", "Subscription value for the interest. 0-normal interest, 1-soft subscribe, 2-hard subscriber, 3-unsubsribe", IntegerValue(2),
                    MakeIntegerAccessor(&Subscriber::m_subscription), MakeIntegerChecker<int32_t>())

      .AddTraceSource("LastRetransmittedInterestDataDelay",
                      "Delay between last retransmitted Interest and received Data",
                      MakeTraceSourceAccessor(&Subscriber::m_lastRetransmittedInterestDataDelay),
                      "ns3::ndn::Subscriber::LastRetransmittedInterestDataDelayCallback")

      .AddTraceSource("FirstInterestDataDelay",
                      "Delay between first transmitted Interest and received Data",
                      MakeTraceSourceAccessor(&Subscriber::m_firstInterestDataDelay),
                      "ns3::ndn::Subscriber::FirstInterestDataDelayCallback");
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
Subscriber::SetRetxTimer(Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning()) {
    // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
    Simulator::Remove(m_retxEvent); // slower, but better for memory
  }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule(m_retxTimer, &Subscriber::CheckRetxTimeout, this);
}

Time
Subscriber::GetRetxTimer() const
{
  return m_retxTimer;
}

void
Subscriber::CheckRetxTimeout()
{
  Time now = Simulator::Now();

  Time rto = m_rtt->RetransmitTimeout();
  // NS_LOG_DEBUG ("Current RTO: " << rto.ToDouble (Time::S) << "s");

  while (!m_seqTimeouts.empty()) {
    SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
      m_seqTimeouts.get<i_timestamp>().begin();
    if (entry->time + rto <= now) // timeout expired?
    {
      uint32_t seqNo = entry->seq;
      m_seqTimeouts.get<i_timestamp>().erase(entry);
      OnTimeout(seqNo);
    }
    else
      break; // nothing else to do. All later packets need not be retransmitted
  }

  m_retxEvent = Simulator::Schedule(m_retxTimer, &Subscriber::CheckRetxTimeout, this);
}

// Application methods

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

  while (m_retxSeqs.size()) {
    seq = *m_retxSeqs.begin();
    m_retxSeqs.erase(m_retxSeqs.begin());
    break;
  }

  if (m_seqMax != std::numeric_limits<uint32_t>::max()) {
      if (m_seq >= m_seqMax) {
        return;
      }
  }

  seq = m_seq++;

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
  //nameWithSequence->appendSequenceNumber(seq); //required to ndn::AppDelayTracer to calculate RTT
  //

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setSubscription(m_subscription);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") > Subscription Interest for " << interest->getName() /*m_interestName*/ );

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

  // This could be a problem......
  //uint32_t seq = data->getName().at(-1).toSequenceNumber();

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") < Received DATA for " << m_interestName /*data->getName()*/);


  int hopCount = 0;
  auto ns3PacketTag = data->getTag<Ns3PacketTag>();
  if (ns3PacketTag != nullptr) { // e.g., packet came from local node's cache
    FwHopCountTag hopCountTag;
    if (ns3PacketTag->getPacket()->PeekPacketTag(hopCountTag)) {
      hopCount = hopCountTag.Get();
      NS_LOG_DEBUG("Hop count: " << hopCount);
    }
  }

/*
  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find(seq);
  if (entry != m_seqLastDelay.end()) {
    m_lastRetransmittedInterestDataDelay(this, seq, Simulator::Now() - entry->time, hopCount);
  }

  entry = m_seqFullDelay.find(seq);
  if (entry != m_seqFullDelay.end()) {
    m_firstInterestDataDelay(this, seq, Simulator::Now() - entry->time, m_seqRetxCounts[seq], hopCount);
  }

  m_seqRetxCounts.erase(seq);
  m_seqFullDelay.erase(seq);
  m_seqLastDelay.erase(seq);

  m_seqTimeouts.erase(seq);
  m_retxSeqs.erase(seq);

  m_rtt->AckSeq(SequenceNumber32(seq));
*/
}

void
Subscriber::OnTimeout(uint32_t sequenceNumber)
{
  //NS_LOG_FUNCTION(sequenceNumber);
  // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
  // m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";

  m_rtt->IncreaseMultiplier(); // Double the next RTO
  m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
                 1); // make sure to disable RTT calculation for this sample
  m_retxSeqs.insert(sequenceNumber);
  ScheduleNextPacket();
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
