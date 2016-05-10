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

#include "ndn-aggregator.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-app-face.hpp"
#include "model/ndn-ns3.hpp"
#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>

#include "utils/ndn-rtt-mean-deviation.hpp"
#include "utils/ndn-ns3-packet-tag.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.Aggregator");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(Aggregator);

TypeId
Aggregator::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::Aggregator")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<Aggregator>()
      .AddAttribute("Prefix", "Prefix, with which aggregator receives payload interest", StringValue("/"),
                    MakeNameAccessor(&Aggregator::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&Aggregator::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&Aggregator::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&Aggregator::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&Aggregator::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&Aggregator::m_keyLocator), MakeNameChecker())

      .AddAttribute("UpstreamPrefix", "Prefix, to which aggregated payloads are sent", StringValue("/"),
                    MakeNameAccessor(&Aggregator::m_upstream_prefix), MakeNameChecker())

      .AddAttribute("LifeTime", "LifeTime for subscription packet", StringValue("1200s"),
                    MakeTimeAccessor(&Aggregator::m_interestLifeTime), MakeTimeChecker())

      .AddAttribute("RetxTimer",
                    "Timeout defining how frequent retransmission timeouts should be checked",
                    StringValue("50ms"),
                    MakeTimeAccessor(&Aggregator::GetRetxTimer, &Aggregator::SetRetxTimer),
                    MakeTimeChecker())

      .AddAttribute("Frequency", "How often payload interests are aggreagted and forwarded to compute nodes",
                    TimeValue(Seconds(1)), MakeTimeAccessor(&Aggregator::m_frequency),
                    MakeTimeChecker());

  return tid;
}

Aggregator::Aggregator()
  : m_totalpayload(0)
  , m_firstTime(true)
  , m_seq(0)
  , m_seqMax(std::numeric_limits<uint32_t>::max())
  , m_rand(CreateObject<UniformRandomVariable>())
{
  NS_LOG_FUNCTION_NOARGS();

  m_rtt = CreateObject<RttMeanDeviation>();
}

// inherited from Application base class
void
Aggregator::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);

  ScheduleAggPackets();
}

void
Aggregator::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
Aggregator::OnInterest(shared_ptr<const Interest> interest)
{

  App::OnInterest(interest); // tracing inside

  //NS_LOG_FUNCTION(this << interest);
  NS_LOG_INFO("node(" << GetNode()->GetId() << ") received: " << interest->getName());

  //Aggregate payload size
  m_totalpayload += interest->getPayloadLength();

}

void
Aggregator::ScheduleAggPackets()
{
	//NS_LOG_FUNCTION_NOARGS();

	if (m_firstTime == true) {
		m_firstTime = false;
		//Schedule next interest with aggreagated payload
		m_txEvent = Simulator::Schedule(Seconds(0.0), &Aggregator::ScheduleAggPackets, this);
	}
	else {
		//Schedule next interest with aggregated payload
                if (!m_txEvent.IsRunning())
			m_txEvent = Simulator::Schedule(m_frequency, &Aggregator::SendAggInterest, this);

	}
}

void
Aggregator::SendAggInterest()
{

//If no payloaded interest received, do not send an interest with zero payload
if (m_totalpayload > 0)
{

  if (!m_active)
    return;

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

  uint8_t payload[1] = {1};

  //
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_upstream_prefix);
  nameWithSequence->appendSequenceNumber(seq);
  //

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  interest->setSubscription(0); //not a subscription interest
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);
  interest->setPayload(payload, m_totalpayload); //concatenated payload

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") > forwarding Interest: " << interest->getName() << " with aggregated Payload = " << m_totalpayload << "bytes");

  WillSendOutInterest(seq);

  m_transmittedInterests(interest, this, m_face);
  m_face->onReceiveInterest(*interest);

  //reset the total payload size to start accumulating subsequent ones
  m_totalpayload = 0;
}

  //Shedule next aggregated payload interest
  ScheduleAggPackets();

}

void
Aggregator::WillSendOutInterest(uint32_t sequenceNumber)
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

Time
Aggregator::GetRetxTimer() const
{
  return m_retxTimer;
}

void
Aggregator::SetRetxTimer(Time retxTimer)
{
  m_retxTimer = retxTimer;
  if (m_retxEvent.IsRunning()) {
    // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
    Simulator::Remove(m_retxEvent); // slower, but better for memory
  }

  // schedule even with new timeout
  m_retxEvent = Simulator::Schedule(m_retxTimer, &Aggregator::CheckRetxTimeout, this);
}

void
Aggregator::CheckRetxTimeout()
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

  m_retxEvent = Simulator::Schedule(m_retxTimer, &Aggregator::CheckRetxTimeout, this);
}

void
Aggregator::OnTimeout(uint32_t sequenceNumber)
{
  m_rtt->IncreaseMultiplier(); // Double the next RTO
  m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
                 1); // make sure to disable RTT calculation for this sample
  m_retxSeqs.insert(sequenceNumber);

  ScheduleAggPackets();
}

//Process incoming ACK packet
void
Aggregator::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  // This could be a problem......
  //uint32_t seq = data->getName().at(-1).toSequenceNumber();

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") < Received ACK for " << /*m_interestName*/ data->getName());

  int hopCount = 0;
  auto ns3PacketTag = data->getTag<Ns3PacketTag>();
  if (ns3PacketTag != nullptr) { // e.g., packet came from local node's cache
    FwHopCountTag hopCountTag;
    if (ns3PacketTag->getPacket()->PeekPacketTag(hopCountTag)) {
      hopCount = hopCountTag.Get();
      NS_LOG_DEBUG("Hop count: " << hopCount);
    }
  }

}


} // namespace ndn
} // namespace ns3
