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

// ndn-simple.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ndnSIM/helper/ndn-link-control-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-fib-helper.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdlib.h>


#include "apps/ndn-app.hpp" //this header is required for Trace Sink

namespace ns3 {


void SentInterestCallbackPhy(uint32_t, shared_ptr<const ndn::Interest>);
void ReceivedInterestCallbackCom(uint32_t, shared_ptr<const ndn::Interest>);

void SentDataCallbackCom(uint32_t, shared_ptr<const ndn::Data>);
void ReceivedDataCallbackPhy(uint32_t, shared_ptr<const ndn::Data>);

void SentInterestCallbackAgg(uint32_t, shared_ptr<const ndn::Interest>);
void ReceivedInterestCallbackAgg(uint32_t, shared_ptr<const ndn::Interest>);

//Trace file
std::ofstream tracefile;

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("0.5ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create(13);

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(1), nodes.Get(2));
  p2p.Install(nodes.Get(2), nodes.Get(0));
  p2p.Install(nodes.Get(1), nodes.Get(9));
  p2p.Install(nodes.Get(3), nodes.Get(9));
  p2p.Install(nodes.Get(3), nodes.Get(10));
  p2p.Install(nodes.Get(10), nodes.Get(2));
  p2p.Install(nodes.Get(1), nodes.Get(11));
  p2p.Install(nodes.Get(11), nodes.Get(4));
  p2p.Install(nodes.Get(2), nodes.Get(12));
  p2p.Install(nodes.Get(4), nodes.Get(12));
  p2p.Install(nodes.Get(5), nodes.Get(3));
  p2p.Install(nodes.Get(3), nodes.Get(6));
  p2p.Install(nodes.Get(4), nodes.Get(8));
  p2p.Install(nodes.Get(4), nodes.Get(7));

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.InstallAll();

  // Choosing forwarding strategy for the various messages

  ndn::StrategyChoiceHelper::InstallAll("/urgent/com/error", "/localhost/nfd/strategy/stateful-fw");
  ndn::StrategyChoiceHelper::InstallAll("/overlay/com/subscription", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/pmu", "/localhost/nfd/strategy/stateful-fw");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/ami", "/localhost/nfd/strategy/stateful-fw");

/*
  ndn::StrategyChoiceHelper::InstallAll("/urgent/com/error", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/overlay/com/subscription", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/pmu", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/ami", "/localhost/nfd/strategy/best-route");
*/
  // Installing applications

  // Subscriber
  //Phy Urgent
  ndn::AppHelper consumerHelper("ns3::ndn::Subscriber");
  consumerHelper.SetPrefix("/urgent/com/error/phy5");
  consumerHelper.SetAttribute("Frequency", StringValue("120"));
  consumerHelper.SetAttribute("Subscription", IntegerValue(0));
  consumerHelper.SetAttribute("PayloadSize", StringValue("15"));
  consumerHelper.Install(nodes.Get(5));

  consumerHelper.SetPrefix("/urgent/com/error/phy6");
  consumerHelper.Install(nodes.Get(6));

  // Phy Subscription
  consumerHelper.SetPrefix("/overlay/com/subscription");
  consumerHelper.SetAttribute("Frequency", StringValue("300"));
  consumerHelper.SetAttribute("Subscription", IntegerValue(1));
  consumerHelper.SetAttribute("LifeTime", StringValue("310"));
  consumerHelper.Install(nodes.Get(7));

  consumerHelper.Install(nodes.Get(8));

  // Phy PMU messages
  consumerHelper.SetPrefix("/direct/agg/pmu/phy5");
  consumerHelper.SetAttribute("Frequency", StringValue("0.004"));
  consumerHelper.SetAttribute("Subscription", IntegerValue(0));
  consumerHelper.SetAttribute("PayloadSize", StringValue("90"));
  consumerHelper.Install(nodes.Get(5));

  consumerHelper.SetPrefix("/direct/agg/pmu/phy6");
  consumerHelper.Install(nodes.Get(6));


  // Phy AMI messages
  consumerHelper.SetPrefix("/direct/agg/ami/phy7");
  consumerHelper.SetAttribute("Frequency", StringValue("1"));
  consumerHelper.SetAttribute("Subscription", IntegerValue(0));
  consumerHelper.SetAttribute("PayloadSize", StringValue("50"));
  consumerHelper.Install(nodes.Get(7));

  consumerHelper.SetPrefix("/direct/agg/ami/phy8");
  consumerHelper.Install(nodes.Get(8));

  // Aggregator

  // Agg PMU
  ndn::AppHelper aggHelper("ns3::ndn::Aggregator");
  aggHelper.SetPrefix("/direct/agg/pmu");
  aggHelper.SetAttribute("UpstreamPrefix", StringValue("/direct/com/pmu"));
  aggHelper.SetAttribute("Frequency",  StringValue("0.004"));
  aggHelper.Install(nodes.Get(3));
  aggHelper.Install(nodes.Get(4));
  aggHelper.Install(nodes.Get(9));
  aggHelper.Install(nodes.Get(10));
  aggHelper.Install(nodes.Get(11));
  aggHelper.Install(nodes.Get(12));

  // Agg PMU
  aggHelper.SetPrefix("/direct/agg/ami");
  aggHelper.SetAttribute("UpstreamPrefix", StringValue("/direct/com/ami"));
  aggHelper.SetAttribute("Frequency",  StringValue("1"));
  aggHelper.Install(nodes.Get(3));
  aggHelper.Install(nodes.Get(4));
  aggHelper.Install(nodes.Get(9));
  aggHelper.Install(nodes.Get(10));
  aggHelper.Install(nodes.Get(11));
  aggHelper.Install(nodes.Get(12));


  // Spontaneous Producer
  ndn::AppHelper producerHelper("ns3::ndn::SpontaneousProducer");
  producerHelper.SetPrefix("/urgent/com/error");
  producerHelper.SetAttribute("Frequency", StringValue("0"));
  producerHelper.Install(nodes.Get(0));
  producerHelper.Install(nodes.Get(1));
  producerHelper.Install(nodes.Get(2));

  producerHelper.SetPrefix("/overlay/com/subscription");
  producerHelper.SetAttribute("Frequency", StringValue("60"));
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(0));
  producerHelper.Install(nodes.Get(1));
  producerHelper.Install(nodes.Get(2));


  // Com PMU
  producerHelper.SetPrefix("/direct/com/pmu");
  producerHelper.SetAttribute("Frequency", StringValue("0"));
  producerHelper.Install(nodes.Get(0));
  producerHelper.Install(nodes.Get(1));
  producerHelper.Install(nodes.Get(2));

  // Com AMI
  producerHelper.SetPrefix("/direct/com/ami");
  producerHelper.SetAttribute("Frequency", StringValue("0"));
  producerHelper.Install(nodes.Get(0));
  producerHelper.Install(nodes.Get(1));
  producerHelper.Install(nodes.Get(2));

  // Configure dynamic routing
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);
  ndnGlobalRoutingHelper.AddOrigin("/urgent/com/error", nodes.Get(0));
  ndnGlobalRoutingHelper.AddOrigin("/urgent/com/error", nodes.Get(1));
  ndnGlobalRoutingHelper.AddOrigin("/urgent/com/error", nodes.Get(2));
  ndnGlobalRoutingHelper.AddOrigin("/overlay/com/subscription", nodes.Get(0));
  ndnGlobalRoutingHelper.AddOrigin("/overlay/com/subscription", nodes.Get(1));
  ndnGlobalRoutingHelper.AddOrigin("/overlay/com/subscription", nodes.Get(2));
  ndnGlobalRoutingHelper.AddOrigin("/direct/com/pmu", nodes.Get(0));
  ndnGlobalRoutingHelper.AddOrigin("/direct/com/pmu", nodes.Get(1));
  ndnGlobalRoutingHelper.AddOrigin("/direct/com/pmu", nodes.Get(2));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/pmu", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/pmu", nodes.Get(4));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/pmu", nodes.Get(9));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/pmu", nodes.Get(10));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/pmu", nodes.Get(11));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/pmu", nodes.Get(12));
  ndnGlobalRoutingHelper.AddOrigin("/direct/com/ami", nodes.Get(0));
  ndnGlobalRoutingHelper.AddOrigin("/direct/com/ami", nodes.Get(1));
  ndnGlobalRoutingHelper.AddOrigin("/direct/com/ami", nodes.Get(2));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/ami", nodes.Get(3));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/ami", nodes.Get(4));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/ami", nodes.Get(9));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/ami", nodes.Get(10));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/ami", nodes.Get(11));
  ndnGlobalRoutingHelper.AddOrigin("/direct/agg/ami", nodes.Get(12));

  ndn::GlobalRoutingHelper::CalculateRoutes();
//  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();


  //Disable and enable the link between two nodes
  Simulator::Schedule(Seconds(100.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(100.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  Simulator::Schedule(Seconds(300.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(300.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  Simulator::Schedule(Seconds(600.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(600.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  Simulator::Schedule(Seconds(900.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(900.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  Simulator::Schedule(Seconds(1200.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(1200.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  Simulator::Schedule(Seconds(1500.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(1500.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  Simulator::Schedule(Seconds(2000.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(2000.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  Simulator::Schedule(Seconds(2200.0), ndn::LinkControlHelper::FailLink, nodes.Get(1), nodes.Get(9));
  Simulator::Schedule(Seconds(2200.5), ndn::LinkControlHelper::UpLink, nodes.Get(1), nodes.Get(9));

  //Open trace file for writing
  tracefile.open("ndn-icens-trace.csv", std::ios::out);
  tracefile << "nodeid, event, name, payloadsize, time" << std::endl;

  std::string strcallback;

  //Trace transmitted payload interest from [physical nodes]
  strcallback = "/NodeList/" + std::to_string(5) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackPhy));

  strcallback = "/NodeList/" + std::to_string(6) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackPhy));

  strcallback = "/NodeList/" + std::to_string(7) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackPhy));

  strcallback = "/NodeList/" + std::to_string(8) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackPhy));

  //Trace received payload interest at [compute nodes]
  strcallback = "/NodeList/" + std::to_string(0) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackCom));

  strcallback = "/NodeList/" + std::to_string(1) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackCom));

  strcallback = "/NodeList/" + std::to_string(2) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackCom));


  //Trace sent data from [compute nodes]
  strcallback = "/NodeList/" + std::to_string(0) + "/ApplicationList/" + "*/SentData";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentDataCallbackCom));

  strcallback = "/NodeList/" + std::to_string(1) + "/ApplicationList/" + "*/SentData";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentDataCallbackCom));

  strcallback = "/NodeList/" + std::to_string(2) + "/ApplicationList/" + "*/SentData";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentDataCallbackCom));


  //Trace received data at [physical nodes]
  strcallback = "/NodeList/" + std::to_string(5) + "/ApplicationList/" + "*/ReceivedData";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedDataCallbackPhy));

  strcallback = "/NodeList/" + std::to_string(6) + "/ApplicationList/" + "*/ReceivedData";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedDataCallbackPhy));

  strcallback = "/NodeList/" + std::to_string(7) + "/ApplicationList/" + "*/ReceivedData";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedDataCallbackPhy));

  strcallback = "/NodeList/" + std::to_string(8) + "/ApplicationList/" + "*/ReceivedData";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedDataCallbackPhy));

  //Trace received interest at [aggregation nodes]
  strcallback = "/NodeList/" + std::to_string(3) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(4) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(9) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(10) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(11) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(12) + "/ApplicationList/" + "*/ReceivedInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));

  //Trace sent interest at [aggregation nodes]
  strcallback = "/NodeList/" + std::to_string(3) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(4) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(9) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(10) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(11) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));

  strcallback = "/NodeList/" + std::to_string(12) + "/ApplicationList/" + "*/SentInterest";
  Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));


  Simulator::Stop(Seconds(3600.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}


//Define callbacks for writing to tracefile
void SentInterestCallbackPhy(uint32_t nodeid, shared_ptr<const ndn::Interest> interest) {
        if (interest->getSubscription() == 1 || interest->getSubscription() == 2) {
                //Do not log subscription interests from phy nodes
        }
        else {
		tracefile << nodeid << ", sent, " << interest->getName() << ", " << interest->getPayloadLength() << ", " << std::fixed << setprecision(9)
			<< (Simulator::Now().GetNanoSeconds())/1000000000.0 << std::endl;
        }
}

void ReceivedInterestCallbackCom(uint32_t nodeid, shared_ptr<const ndn::Interest> interest) {
        if (interest->getSubscription() == 1 || interest->getSubscription() == 2) {
                //Do not log subscription interests received at com nodes
        }
        else {
                tracefile << nodeid << ", recv, " << interest->getName() << ", " << interest->getPayloadLength() << ", " << std::fixed << setprecision(9)
			 << (Simulator::Now().GetNanoSeconds())/1000000000.0 << std::endl;
        }
}

void SentDataCallbackCom(uint32_t nodeid, shared_ptr<const ndn::Data> data) {
	if (
		(data->getName().toUri()).find("error") != std::string::npos ||
		(data->getName().toUri()).find("pmu") != std::string::npos ||
		(data->getName().toUri()).find("ami") != std::string::npos

	) {
		//Do not log data for error, PMU ACK, AMI ACK from compute
	}
	else {
		tracefile << nodeid << ", sent, " << data->getName() << ", 1024, " << std::fixed << setprecision(9) << (Simulator::Now().GetNanoSeconds())/1000000000.0 << std::endl;
	}
}

void ReceivedDataCallbackPhy(uint32_t nodeid, shared_ptr<const ndn::Data> data) {
	if ((data->getName().toUri()).find("error") != std::string::npos) {
		//Do not log data received in response to payload interest
	}
	else {
		tracefile << nodeid << ", recv, " << data->getName() << ", 1024, " << std::fixed << setprecision(9) << (Simulator::Now().GetNanoSeconds())/1000000000.0 << std::endl;
	}
}

void ReceivedInterestCallbackAgg(uint32_t nodeid, shared_ptr<const ndn::Interest> interest) {
        tracefile << nodeid << ", recv, " << interest->getName() << ", " << interest->getPayloadLength() << ", " << std::fixed << setprecision(9)
		<< (Simulator::Now().GetNanoSeconds())/1000000000.0 << std::endl;
}

void SentInterestCallbackAgg(uint32_t nodeid, shared_ptr<const ndn::Interest> interest) {
        tracefile << nodeid << ", sent, " << interest->getName() << ", " << interest->getPayloadLength() << ", " << std::fixed << setprecision(9)
		<< (Simulator::Now().GetNanoSeconds())/1000000000.0 << std::endl;
}



} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
