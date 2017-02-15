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

#include "ndn-producer-spontaneous.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>
#include <fstream>

NS_LOG_COMPONENT_DEFINE("ndn.SpontaneousProducer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(SpontaneousProducer);

TypeId
SpontaneousProducer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::SpontaneousProducer")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<SpontaneousProducer>()
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&SpontaneousProducer::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&SpontaneousProducer::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&SpontaneousProducer::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&SpontaneousProducer::m_freshness),
                    MakeTimeChecker())
      .AddAttribute("Frequency", "Frequency of data packets, if 0, then no spontaneous publish",
                    TimeValue(Seconds(5)), MakeTimeAccessor(&SpontaneousProducer::m_frequency),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&SpontaneousProducer::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&SpontaneousProducer::m_keyLocator), MakeNameChecker())

      .AddTraceSource("SentData", "SentData",
                      MakeTraceSourceAccessor(&SpontaneousProducer::m_sentData),
                      "ns3::ndn::SpontaneousProducer::SentDataTraceCallback")

      .AddTraceSource("ReceivedInterest", "ReceivedInterest",
                      MakeTraceSourceAccessor(&SpontaneousProducer::m_receivedInterest),
                      "ns3::ndn::SpontaneousProducer::ReceivedInterestTraceCallback");

  return tid;
}

SpontaneousProducer::SpontaneousProducer()
  :m_firstTime(true)
  , m_subscription(0)
  , m_receivedpayload(0)
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
SpontaneousProducer::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  m_prefixWithoutSequence = m_prefix; //store prefix without sequence using to be used in log output

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);

  SendTimeout();
}

void
SpontaneousProducer::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
SpontaneousProducer::OnInterest(shared_ptr<const Interest> interest)
{

  NS_LOG_INFO("SUBSCRIPTION value = " << interest->getSubscription() << " & PAYLOAD = " << interest->getPayloadLength() << " TIME: " << Simulator::Now());

  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);

  // Callback for received interests
  m_receivedInterest(GetNode()->GetId(), interest);

  if (!m_active)
    return;

  //Send data if there's a subscription
  m_subscription = interest->getSubscription();
  m_receivedpayload = interest->getPayloadLength();

  m_prefix = interest->getName();

  //Normal interest, without a subscription
  if (m_subscription == 0) {
        SendData(m_prefix);
  }

}

void
SpontaneousProducer::SendTimeout(){
        //Do not send initial data before scheduling with the input frequency
        if(m_firstTime) {
            m_firstTime = false;
        }
        else {
	    //Only send data when there is a subscription (1-soft or 2-hard)
	    if (m_subscription == 1 || m_subscription == 2)
	    	SendData(m_prefix);
	}

        if(m_frequency != 0){
       		m_txEvent = Simulator::Schedule(m_frequency, &SpontaneousProducer::SendTimeout, this);
        }

}

void
SpontaneousProducer::SendData(const Name &dataName)
{

  if (!m_active)
    return;

  //send only ACK (payload size = 0) if payloaded interest was received
  if (m_subscription == 0 && m_receivedpayload > 0) {
	m_virtualPayloadSize = 0;
  }

  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));
  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);

  if (m_subscription == 0 && m_receivedpayload > 0) {
  	NS_LOG_INFO("node(" << GetNode()->GetId() << ") sending ACK: " << data->getName() << " TIME: " << Simulator::Now());
  }
  else {
	NS_LOG_INFO("node(" << GetNode()->GetId() << ") sending DATA for " << data->getName() << " TIME: " << Simulator::Now());
  }

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);

  // Callback for tranmitted subscription data
  m_sentData(GetNode()->GetId(), data);

}

} // namespace ndn
} // namespace ns3
