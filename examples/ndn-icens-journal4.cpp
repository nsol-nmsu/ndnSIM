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

bool NodeInComm(int);
bool NodeInAgg(int);

// Vectors to store the various node types
vector<int> com_nodes, agg_nodes, phy_nodes;

// Vectors to store source and destination edges to fail
vector<int> srcedge, dstedge;

//Trace file
std::ofstream tracefile;

int
main(int argc, char* argv[])
{

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

  //--- Count the number of nodes to create
  ifstream nfile ("src/ndnSIM/examples/icens-nodes2.txt", std::ios::in);

  std::string nodeid, nodename, nodetype;
  int nodecount = 0; //number of nodes in topology
  int numOfPMUs = 20; //number of PMUs

  if (nfile.is_open ()) {
  	while (nfile >> nodeid >> nodename >> nodetype) {

		nodecount += 1;

		if (nodename.substr(0,4) == "com_") {
			com_nodes.push_back(std::stoi(nodeid));
		}
		else if (nodename.substr(0,4) == "agg_") {
			agg_nodes.push_back(std::stoi(nodeid));
		}
		else if (nodename.substr(0,4) == "phy_") {
			phy_nodes.push_back(std::stoi(nodeid));
		}
		else {
			cout << "Error::Node not classified" << endl;
		}
  	}
  }
  else {
	std::cout << "Error::Cannot open nodes file!!!" << std::endl;
	return 1;
  }
  nfile.close();

  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("10"));

  // Creating the number of nodes counted in the nodes file
  NodeContainer nodes;
  nodes.Create(nodecount);

  // Connecting nodes using two links
  PointToPointHelper p2p;

  //--- Get the edges of the graph from file and connect them
  ifstream efile ("src/ndnSIM/examples/icens-edges2.txt", std::ios::in);

  std::string srcnode, dstnode, bw, delay, edgetype;

  if (efile.is_open ()) {
        while (efile >> srcnode >> dstnode >> bw >> delay >> edgetype) {
		//Set delay and bandwidth parameters for point-to-point links
		p2p.SetDeviceAttribute("DataRate", StringValue(bw+"Mbps"));
		p2p.SetChannelAttribute("Delay", StringValue(delay+"ms"));
		p2p.Install(nodes.Get(std::stoi(srcnode)), nodes.Get(std::stoi(dstnode)));

		// Determine if an edge is between compute and aggregation node - some of these edges will be failed during simulation
		if (NodeInComm(stoi(srcnode)) && NodeInAgg(stoi(dstnode))) {
			srcedge.push_back(stoi(srcnode));
			dstedge.push_back(stoi(dstnode));
		}
                if (NodeInAgg(stoi(srcnode)) && NodeInComm(stoi(dstnode))) {
			srcedge.push_back(stoi(srcnode));
                        dstedge.push_back(stoi(dstnode));
		}
        }
  }
  else {
        std::cout << "Error::Cannot open edges file!!!" << std::endl;
	return 1;
  }
  efile.close();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.InstallAll();

  // Setup dynamic routing
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.Install(nodes);

  // Install server applications on compute nodes
  ndn::AppHelper producerHelper("ns3::ndn::SpontaneousProducer");
  for (int i=0; i<(int)com_nodes.size(); i++) {
	// Urgent messages (Error Reporting)
 	producerHelper.SetPrefix("/urgent/com/error");
  	producerHelper.SetAttribute("Frequency", StringValue("0"));
	producerHelper.Install(nodes.Get(com_nodes[i]));
	// PMU messages
	producerHelper.SetPrefix("/direct/com/pmu");
	producerHelper.SetAttribute("Frequency", StringValue("0"));
	producerHelper.Install(nodes.Get(com_nodes[i]));
	// AMI messages
	producerHelper.SetPrefix("/direct/com/ami");
  	producerHelper.SetAttribute("Frequency", StringValue("0"));
  	producerHelper.Install(nodes.Get(com_nodes[i]));
	// Subscription messages
  	producerHelper.SetPrefix("/overlay/com/subscription");
  	producerHelper.SetAttribute("Frequency", StringValue("60"));
  	producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
	producerHelper.Install(nodes.Get(com_nodes[i]));

	// Setup node to originate prefixes for dynamic routing
	ndnGlobalRoutingHelper.AddOrigin("/urgent/com/error", nodes.Get(com_nodes[i]));
	ndnGlobalRoutingHelper.AddOrigin("/overlay/com/subscription", nodes.Get(com_nodes[i]));
	ndnGlobalRoutingHelper.AddOrigin("/direct/com/pmu", nodes.Get(com_nodes[i]));
	ndnGlobalRoutingHelper.AddOrigin("/direct/com/ami", nodes.Get(com_nodes[i]));
  }

  // Install aggregator application on aggregation nodes
  ndn::AppHelper aggHelper("ns3::ndn::Aggregator");
  for (int i=0; i<(int)agg_nodes.size(); i++) {
	// PMU meesages
  	aggHelper.SetPrefix("/direct/agg/pmu");
  	aggHelper.SetAttribute("UpstreamPrefix", StringValue("/direct/com/pmu"));
  	aggHelper.SetAttribute("Frequency",  StringValue("0.004"));
	aggHelper.SetAttribute("Offset", IntegerValue(0));
	aggHelper.SetAttribute("LifeTime", StringValue("10"));
  	aggHelper.Install(nodes.Get(agg_nodes[i]));
	// AMI messages
  	aggHelper.SetPrefix("/direct/agg/ami");
  	aggHelper.SetAttribute("UpstreamPrefix", StringValue("/direct/com/ami"));
  	aggHelper.SetAttribute("Frequency",  StringValue("1"));
	int offset = (rand() % 900) + 100;
	aggHelper.SetAttribute("Offset", IntegerValue(offset));
	aggHelper.SetAttribute("LifeTime", StringValue("10"));
  	aggHelper.Install(nodes.Get(agg_nodes[i]));

	// Setup node to originate prefixes for dynamic routing
	ndnGlobalRoutingHelper.AddOrigin("/direct/agg/pmu", nodes.Get(agg_nodes[i]));
	ndnGlobalRoutingHelper.AddOrigin("/direct/agg/ami", nodes.Get(agg_nodes[i]));
  }

  // Choosing forwarding strategy for the various messages

  ndn::StrategyChoiceHelper::InstallAll("/urgent/com/error", "/localhost/nfd/strategy/multicast");
  ndn::StrategyChoiceHelper::InstallAll("/overlay/com/subscription", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/pmu", "/localhost/nfd/strategy/stateful-fw");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/ami", "/localhost/nfd/strategy/stateful-fw");

/*
  ndn::StrategyChoiceHelper::InstallAll("/urgent/com/error", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/overlay/com/subscription", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/pmu", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/ami", "/localhost/nfd/strategy/best-route");
*/

  // Installing client applications on physical layer nodes
  ndn::AppHelper consumerHelper("ns3::ndn::Subscriber");

  // Urgent messages are sent by PMUs to compute nodes for error reporting using - "/urgent/com/error"

  // Seed for random offset
  srand(20);

  for (int i=0; i<(int)phy_nodes.size(); i++) {
	if (i < numOfPMUs) {
		consumerHelper.SetPrefix("/urgent/com/error/phy" + std::to_string(phy_nodes[i]));
                consumerHelper.SetAttribute("Frequency", StringValue("120"));
                consumerHelper.SetAttribute("Subscription", IntegerValue(0));
                consumerHelper.SetAttribute("PayloadSize", StringValue("60"));
		int offset = (rand() % 91) + 1; //random offset between 1 and 91
	        consumerHelper.SetAttribute("Offset", IntegerValue(offset));
		consumerHelper.SetAttribute("LifeTime", StringValue("10"));
                consumerHelper.Install(nodes.Get(phy_nodes[i]));
	}
  }

  // PMUs sends payload interests to aggregation layer - "/direct/com/pmu"
  for (int i=0; i<(int)phy_nodes.size(); i++) {
        if (i < numOfPMUs) {
                consumerHelper.SetPrefix("/direct/agg/pmu/phy" + std::to_string(phy_nodes[i]));
                consumerHelper.SetAttribute("Frequency", StringValue("0.004"));
                consumerHelper.SetAttribute("Subscription", IntegerValue(0));
                consumerHelper.SetAttribute("PayloadSize", StringValue("90"));
                consumerHelper.SetAttribute("RetransmitPackets", IntegerValue(0));
                consumerHelper.SetAttribute("Offset", IntegerValue(0));
                consumerHelper.SetAttribute("LifeTime", StringValue("10"));
                consumerHelper.Install(nodes.Get(phy_nodes[i]));
        }
  }

  // AMIs sends payload interests to aggregation layer - "/direct/com/ami"
  for (int i=0; i<(int)phy_nodes.size(); i++) {
        if (i >= numOfPMUs) {
                consumerHelper.SetPrefix("/direct/agg/ami/phy" + std::to_string(phy_nodes[i]));
                consumerHelper.SetAttribute("Frequency", StringValue("1"));
                consumerHelper.SetAttribute("Subscription", IntegerValue(0));
                consumerHelper.SetAttribute("PayloadSize", StringValue("60"));
                consumerHelper.SetAttribute("Offset", IntegerValue(0));
                consumerHelper.SetAttribute("LifeTime", StringValue("10"));
                consumerHelper.Install(nodes.Get(phy_nodes[i]));
        }
  }

  // Each physical layer node except PMUs, subscribe to compute prefix - "/overlay/com/subscription"
  for (int i=0; i<(int)phy_nodes.size(); i++) {
        if (i >= numOfPMUs) {
                consumerHelper.SetPrefix("/overlay/com/subscription");
                consumerHelper.SetAttribute("Frequency", StringValue("300"));
                consumerHelper.SetAttribute("Subscription", IntegerValue(1));
                consumerHelper.SetAttribute("Offset", IntegerValue(0));
                consumerHelper.SetAttribute("LifeTime", StringValue("310"));
                consumerHelper.Install(nodes.Get(phy_nodes[i]));
        }
  }

  // Populate routing table for nodes
  //ndn::GlobalRoutingHelper::CalculateRoutes();
  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();

  bool DisableLink = true;
  //Disable and enable half of the links between com to agg nodes, for a number of times during simulation
  for (int i=0; i<(int)srcedge.size(); i++) {
	if (DisableLink) {

		for (int j=0; j<360; j++) {
			int offset = (rand() % 3599) + 1;
	                Simulator::Schedule(Seconds( ((double)offset) ), ndn::LinkControlHelper::FailLink, nodes.Get(srcedge[i]), nodes.Get(dstedge[i]));
                	Simulator::Schedule(Seconds( ((double)offset + 0.1) ), ndn::LinkControlHelper::UpLink, nodes.Get(srcedge[i]), nodes.Get(dstedge[i]));
		}

		DisableLink = false;
	}
	else {
		DisableLink = true;
	}
  }

  //Open trace file for writing
  tracefile.open("ndn-icens-trace.csv", std::ios::out);
  tracefile << "nodeid, event, name, payloadsize, time" << std::endl;

  std::string strcallback;

  //Trace transmitted payload interest from [physical nodes]
  for (int i=0; i<(int)phy_nodes.size(); i++) {
  	strcallback = "/NodeList/" + std::to_string(phy_nodes[i]) + "/ApplicationList/" + "*/SentInterest";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackPhy));
  }

  //Trace received payload interest at [compute nodes]
  for (int i=0; i<(int)com_nodes.size(); i++) {
  	strcallback = "/NodeList/" + std::to_string(com_nodes[i]) + "/ApplicationList/" + "*/ReceivedInterest";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackCom));
  }

  //Trace sent data from [compute nodes]
  for (int i=0; i<(int)com_nodes.size(); i++) {
  	strcallback = "/NodeList/" + std::to_string(com_nodes[i]) + "/ApplicationList/" + "*/SentData";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&SentDataCallbackCom));
  }

  //Trace received data at [physical nodes]
  for (int i=0; i<(int)phy_nodes.size(); i++) {
  	strcallback = "/NodeList/" + std::to_string(phy_nodes[i]) + "/ApplicationList/" + "*/ReceivedData";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedDataCallbackPhy));
  }

  //Trace received interest at [aggregation nodes]
  for (int i=0; i<(int)agg_nodes.size(); i++) {
  	strcallback = "/NodeList/" + std::to_string(agg_nodes[i]) + "/ApplicationList/" + "*/ReceivedInterest";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));
  }

  //Trace sent interest at [aggregation nodes]
  for (int i=0; i<(int)agg_nodes.size(); i++) {
  	strcallback = "/NodeList/" + std::to_string(agg_nodes[i]) + "/ApplicationList/" + "*/SentInterest";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));
  }

  Simulator::Stop(Seconds(7200.0));
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

void GetComAggEdges() {

}

bool NodeInComm(int nodeid) {
	for (int i=0; i<(int)com_nodes.size(); i++) {
		if (com_nodes[i] == nodeid) {
			return true;
		}
	}
	return false;
}

bool NodeInAgg(int nodeid) {
        for (int i=0; i<(int)agg_nodes.size(); i++) {
                if (agg_nodes[i] == nodeid) {
                        return true;
                }
        }
        return false;
}


} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
