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

namespace ns3 {

/**
 * This scenario simulates a very simple network topology:
 *
 *
 *      +----------+     1Mbps      +--------+     1Mbps      +----------+
 *      | consumer | <------------> | router | <------------> | producer |
 *      +----------+         10ms   +--------+          10ms  +----------+
 *
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-simple
 */

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
  nodes.Create(6);

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(1), nodes.Get(2));
  p2p.Install(nodes.Get(2), nodes.Get(3));
  p2p.Install(nodes.Get(3), nodes.Get(4));
  p2p.Install(nodes.Get(2), nodes.Get(5));
  p2p.Install(nodes.Get(3), nodes.Get(5));

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  //ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/stateful-fw");

  // Installing applications

  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::Subscriber");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix("/prefix/phy0");
  consumerHelper.SetAttribute("Frequency", StringValue("1")); // 10 interests a second
  consumerHelper.SetAttribute("Subscription", IntegerValue(0));
  consumerHelper.SetAttribute("PayloadSize", StringValue("15"));
  consumerHelper.Install(nodes.Get(0));                        // first node

/*
  consumerHelper.SetPrefix("/prefix/agg/phy6");
  consumerHelper.SetAttribute("Frequency", StringValue("1")); // 10 interests a second
  consumerHelper.SetAttribute("Subscription", IntegerValue(0));
  consumerHelper.SetAttribute("PayloadSize", StringValue("7"));
  consumerHelper.Install(nodes.Get(6));


  // Aggregator
  ndn::AppHelper aggH("ns3::ndn::Aggregator");
  // Consumer will request /prefix/0, /prefix/1, ...
  aggH.SetPrefix("/prefix/agg");
  aggH.SetAttribute("UpstreamPrefix", StringValue("/prefix/com")); //forward to com node prefix
  aggH.SetAttribute("Frequency",  StringValue("1")); ////wait seconds before aggregatiion
  aggH.Install(nodes.Get(1));
*/
  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::SpontaneousProducer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix("/prefix");
  producerHelper.SetAttribute("Frequency", StringValue("0"));
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(4)); // last node

/*
  //Static routes
  ndn::FibHelper::AddRoute(nodes.Get(0), "/prefix", nodes.Get(1), 1);
  ndn::FibHelper::AddRoute(nodes.Get(1), "/prefix", nodes.Get(2), 1);
  ndn::FibHelper::AddRoute(nodes.Get(2), "/prefix", nodes.Get(3), 1);
  ndn::FibHelper::AddRoute(nodes.Get(3), "/prefix", nodes.Get(4), 1);
*/

//dynamic routing
ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
ndnGlobalRoutingHelper.Install(nodes);
ndnGlobalRoutingHelper.AddOrigin("/prefix", nodes.Get(4));
//ndn::GlobalRoutingHelper::CalculateRoutes();
ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();


  //Disable the link between two nodes
  Simulator::Schedule(Seconds(1.0), ndn::LinkControlHelper::FailLink, nodes.Get(2), nodes.Get(3));
  Simulator::Schedule(Seconds(1.0), ndn::FibHelper::RemoveRouteStr, nodes.Get(2), "/prefix", nodes.Get(3));
  Simulator::Schedule(Seconds(1.0), ndn::FibHelper::RemoveRouteStr, nodes.Get(3), "/prefix", nodes.Get(2));

  //Disable the link between two nodes
  Simulator::Schedule(Seconds(1.6), ndn::LinkControlHelper::UpLink, nodes.Get(2), nodes.Get(3));
  Simulator::Schedule(Seconds(1.6), ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes);


  Simulator::Stop(Seconds(3.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}