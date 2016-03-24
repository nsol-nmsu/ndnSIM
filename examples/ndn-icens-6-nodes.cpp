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

// ndn-congestion-topo-plugin.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

/**
 * This scenario simulates a grid topology (using topology reader module)
 *
 *   /------\                                                    /------\
 *   | Pro1 |<--+                                            +-->| Pro2 |
 *   \------/    \                                          /    \------/
 *                \                                        /
 *                 +-->/------\                 /------\<-+
 *                     | Rtr1 |<===============>| Rtr2 |
 *                 +-->\------/                 \------/<-+
 *                /                                        \
 *   /------\    /                                          \    /------\
 *   | Con1 |<--+                                            +-->| Con2 |
 *   \------/                                                    \------/
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-congestion-topo-plugin
 */

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  AnnotatedTopologyReader topologyReader("", 25);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/icens-6-nodes.txt");
  topologyReader.Read();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "10000");
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/best-route");

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  // Getting containers for the consumer/producer
  Ptr<Node> consumer1 = Names::Find<Node>("Con1");
  Ptr<Node> consumer2 = Names::Find<Node>("Con2");

  Ptr<Node> producer1 = Names::Find<Node>("Pro1");
  Ptr<Node> producer2 = Names::Find<Node>("Pro2");


  ndn::AppHelper consumerHelper("ns3::ndn::Subscriber");
  //consumerHelper.SetAttribute("Frequency", StringValue("1")); // 1 interest a second
  consumerHelper.SetAttribute("TxTimer", StringValue("2000")); //resend subscription interest every 5 seconds

  // on the first consumer node install a Consumer application
  // that will express interests in /data1 namespace
  consumerHelper.SetPrefix("/data1");
  consumerHelper.Install(consumer1);

  // on the first consumer node install a Consumer application
  // that will express interests in /data2 namespace
  consumerHelper.SetPrefix("/data2");
  consumerHelper.Install(consumer1);



  // on the second consumer node install a Consumer application
  // that will express interests in /data1 namespace
  consumerHelper.SetPrefix("/data1");
  consumerHelper.Install(consumer2);

  // on the second consumer node install a Consumer application
  // that will express interests in /data2 namespace
  consumerHelper.SetPrefix("/data2");
  consumerHelper.Install(consumer2);


  ndn::AppHelper producerHelper("ns3::ndn::SpontaneousProducer");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.SetAttribute("Frequency", StringValue("1")); //how often (in seconds) subscribed data is sent

  // Register /data1 prefix with global routing controller and
  // install producer that will satisfy Interests in /data1 namespace
  ndnGlobalRoutingHelper.AddOrigins("/data1", producer1);
  producerHelper.SetPrefix("/data1");
  producerHelper.Install(producer1);

  // Register /data2 prefix with global routing controller and
  // install producer that will satisfy Interests in /data2 namespace
  ndnGlobalRoutingHelper.AddOrigins("/data2", producer2);
  producerHelper.SetPrefix("/data2");
  producerHelper.Install(producer2);

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes();

  Simulator::Stop(Seconds(1000.0));

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
