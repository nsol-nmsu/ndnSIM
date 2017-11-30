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

// Split a line into a vector of strings
std::vector<std::string> SplitString(std::string);

//Get source node ID from which payload interest came from
uint32_t GetSourceNodeID (std::string name);

//Check if source/destination flow is permitted in config file (one-to-many, many-to-one flows)
bool FlowPermitted (int dstnode, int srcnode);

//Store unique IDs to prevent repeated installation of app
bool IsPMUAppInstalled(std::string PMUID);
bool IsPDCAppInstalled(std::string PDCID);
bool IsWACAppInstalled(std::string WACID);

std::vector<std::string> uniquePMUs;
std::vector<std::string> uniquePDCs;
std::vector<std::string> uniqueWACs;

std::pair<int,int> pmu_nodes;
std::pair<int,int> pdc_nodes;
std::pair<int,int> wac_nodes;

std::vector<std::pair<int,int>> all_flows;

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

  // Open the configuration file for reading
  ifstream configFile ("src/ndnSIM/examples/case39Cyber.txt", std::ios::in);

  std::string strLine;
  bool gettingNodeCount = false, buildingNetworkTopo = false, attachingWACs = false, attachingPMUs = false, attachingPDCs = false, flowPMUtoPDC = false, flowPMUtoWAC = false;
  std::vector<std::string> netParams;

  NodeContainer nodes;
  int nodeCount = 0;
  std::pair<int,int> flow_pair;

  // Setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("10"));

  PointToPointHelper p2p;

  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;

  ndn::AppHelper producerHelper("ns3::ndn::SpontaneousProducer");
  ndn::AppHelper consumerHelper("ns3::ndn::Subscriber");

  if (configFile.is_open ()) {
        while (std::getline(configFile, strLine)) {

                //Determine what operation is ongoing while reading the config file
		if(strLine.substr(0,7) == "BEG_000") { gettingNodeCount = true; continue; }
		if(strLine.substr(0,7) == "END_000") {
			//Create nodes
			nodeCount++; //Counter increment needed because nodes start from 1 not 0
			gettingNodeCount = false;
			nodes.Create(nodeCount);
			
			continue; 
		}
		if(strLine.substr(0,7) == "BEG_001") { buildingNetworkTopo = true; continue; }
		if(strLine.substr(0,7) == "END_001") { buildingNetworkTopo = false; continue; }
		if(strLine.substr(0,7) == "BEG_002") { attachingWACs = true; continue; }
		if(strLine.substr(0,7) == "END_002") { attachingWACs = false; continue; }
		if(strLine.substr(0,7) == "BEG_003") { attachingPMUs = true; continue; }
		if(strLine.substr(0,7) == "END_003") { attachingPMUs = false; continue; }
		if(strLine.substr(0,7) == "BEG_004") { attachingPDCs = true; continue; }
		if(strLine.substr(0,7) == "END_004") { 
			attachingPDCs = false; 
			ndn::StackHelper ndnHelper; 
			ndnHelper.InstallAll();

			ndnGlobalRoutingHelper.Install(nodes);
 
			ndn::StrategyChoiceHelper::InstallAll("/power/pdc", "/localhost/nfd/strategy/multicast");
			ndn::StrategyChoiceHelper::InstallAll("/power/wac", "/localhost/nfd/strategy/multicast");

			continue; 
		}
		if(strLine.substr(0,7) == "BEG_005") { flowPMUtoPDC = true; continue; }
		if(strLine.substr(0,7) == "END_005") { flowPMUtoPDC = false; uniquePMUs.clear(); continue; }
		if(strLine.substr(0,7) == "BEG_006") { flowPMUtoWAC = true; continue; }
		if(strLine.substr(0,7) == "END_006") { flowPMUtoWAC = false; continue; }

		if(gettingNodeCount == true) {
                        //Getting number of nodes to create
                        netParams = SplitString(strLine);
			nodeCount = stoi(netParams[1]);

			//Store node ID ranges for various node types
			if(netParams[2] == "wacs") { wac_nodes.first = std::stoi(netParams[0]); wac_nodes.second = std::stoi(netParams[1]); } 
			if(netParams[2] == "pmus") { pmu_nodes.first = std::stoi(netParams[0]); pmu_nodes.second = std::stoi(netParams[1]); }
			if(netParams[2] == "pdcs") { pdc_nodes.first = std::stoi(netParams[0]); pdc_nodes.second = std::stoi(netParams[1]); }
		}
		else if(buildingNetworkTopo == true) {
                        //Building network topology
                        netParams = SplitString(strLine);
                        p2p.SetDeviceAttribute("DataRate", StringValue(netParams[2]+"Mbps"));
                        p2p.SetChannelAttribute("Delay", StringValue(netParams[3]+"ms"));
          		p2p.Install(nodes.Get(std::stoi(netParams[0])), nodes.Get(std::stoi(netParams[1])));
		}
		else if(attachingWACs == true) {
                        //Attaching WACs to routers
                        netParams = SplitString(strLine);
                        p2p.SetDeviceAttribute("DataRate", StringValue(netParams[2]+"Mbps"));
                        p2p.SetChannelAttribute("Delay", StringValue(netParams[3]+"ms"));
                        p2p.Install(nodes.Get(std::stoi(netParams[0])), nodes.Get(std::stoi(netParams[1])));
		}
		else if(attachingPMUs == true) {
                        //Attaching PMUs to routers
                        netParams = SplitString(strLine);
                        p2p.SetDeviceAttribute("DataRate", StringValue(netParams[2]+"Mbps"));
                        p2p.SetChannelAttribute("Delay", StringValue(netParams[3]+"ms"));
                        p2p.Install(nodes.Get(std::stoi(netParams[0])), nodes.Get(std::stoi(netParams[1])));
		}
		else if(attachingPDCs == true) {
                        //Attaching PDCs to routers
                        netParams = SplitString(strLine);
                        p2p.SetDeviceAttribute("DataRate", StringValue(netParams[2]+"Mbps"));
                        p2p.SetChannelAttribute("Delay", StringValue(netParams[3]+"ms"));
                        p2p.Install(nodes.Get(std::stoi(netParams[0])), nodes.Get(std::stoi(netParams[1])));
		}
		else if(flowPMUtoPDC == true) {
			//Install apps on PDCs and PMUs for data exchange
			netParams = SplitString(strLine);

			//Install app on unique PDC IDs
			if (IsPDCAppInstalled(netParams[0]) == false) {
				producerHelper.SetPrefix("/power/pdc");
        			producerHelper.SetAttribute("Frequency", StringValue("0"));
        			producerHelper.Install(nodes.Get(std::stoi(netParams[0])));

				// Setup node to originate prefixes for dynamic routing
  				ndnGlobalRoutingHelper.AddOrigin("/power/pdc", nodes.Get(std::stoi(netParams[0])));
			}

			//Install flow app on PMUs to send data to PDCs
			if (IsPMUAppInstalled(netParams[1]) == false) {
        			consumerHelper.SetPrefix("/power/pdc/phy" + netParams[1]);
                		consumerHelper.SetAttribute("Frequency", StringValue("2")); //0.016
                		consumerHelper.SetAttribute("Subscription", IntegerValue(0));
                		consumerHelper.SetAttribute("PayloadSize", StringValue("200"));
                		consumerHelper.SetAttribute("RetransmitPackets", IntegerValue(0));
                		consumerHelper.SetAttribute("Offset", IntegerValue(0));
                		consumerHelper.SetAttribute("LifeTime", StringValue("10"));
                		consumerHelper.Install(nodes.Get(std::stoi(netParams[1])));
			}

			//Save the flow
			flow_pair.first = stoi(netParams[0]);
			flow_pair.second = stoi(netParams[1]);
			all_flows.push_back(flow_pair);
		}
		else if(flowPMUtoWAC == true) {

                        //Install apps on PDCs and PMUs for data exchange
                        netParams = SplitString(strLine);

                        //Install app on unique PDC IDs
                        if (IsWACAppInstalled(netParams[0]) == false) {
                                producerHelper.SetPrefix("/power/wac");
                                producerHelper.SetAttribute("Frequency", StringValue("0"));
                                producerHelper.Install(nodes.Get(std::stoi(netParams[0])));

                                // Setup node to originate prefixes for dynamic routing
                                ndnGlobalRoutingHelper.AddOrigin("/power/wac", nodes.Get(std::stoi(netParams[0])));
                        }

                        //Install flow app on PMUs to send data to WACs
                        if (IsPMUAppInstalled(netParams[1]) == false) {
                                consumerHelper.SetPrefix("/power/wac/phy" + netParams[1]);
                                consumerHelper.SetAttribute("Frequency", StringValue("2")); //0.016
                                consumerHelper.SetAttribute("Subscription", IntegerValue(0));
                                consumerHelper.SetAttribute("PayloadSize", StringValue("200"));
                                consumerHelper.SetAttribute("RetransmitPackets", IntegerValue(0));
                                consumerHelper.SetAttribute("Offset", IntegerValue(0));
                                consumerHelper.SetAttribute("LifeTime", StringValue("10"));
                                consumerHelper.Install(nodes.Get(std::stoi(netParams[1])));
                        } 

			//Save the flow
                        flow_pair.first = stoi(netParams[0]);
                        flow_pair.second = stoi(netParams[1]);
                        all_flows.push_back(flow_pair);

             }
		else {
			//std::cout << "reading something else " << strLine << std::endl;
		}	
 
	} //end while
  } //endif
  else {
	std::cout << "Cannot open configuration file!!!" << std::endl;
	exit(1);
  }

  configFile.close();

  // Populate routing table for nodes
  //ndn::GlobalRoutingHelper::CalculateRoutes();
  ndn::GlobalRoutingHelper::CalculateAllPossibleRoutes();


/*
  ndn::StrategyChoiceHelper::InstallAll("/urgent/com/error", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/overlay/com/subscription", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/pmu", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/direct/com/ami", "/localhost/nfd/strategy/best-route");
*/

/*
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
*/

  //Open trace file for writing
  tracefile.open("ndn-case39cyber-trace.csv", std::ios::out);
  tracefile << "nodeid, event, name, payloadsize, time" << std::endl;

  std::string strcallback;

  //Trace transmitted payload interest from [PMU nodes]
  for (int i=pmu_nodes.first; i<(pmu_nodes.second + 1); i++) {
  	strcallback = "/NodeList/" + std::to_string(i) + "/ApplicationList/" + "*/SentInterest";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackPhy));
  }

  //Trace received payload interest at [PDC nodes]
  for (int i=pdc_nodes.first; i<(pdc_nodes.second + 1); i++) {
  	strcallback = "/NodeList/" + std::to_string(i) + "/ApplicationList/" + "*/ReceivedInterest";
  	Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackCom));
  }


  //Trace received payload interest at [WAC nodes]
  for (int i=wac_nodes.first; i<(wac_nodes.second + 1) ; i++) {
        strcallback = "/NodeList/" + std::to_string(i) + "/ApplicationList/" + "*/ReceivedInterest";
        Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackCom));
  }



  //Trace sent data from [compute nodes]
  //for (int i=0; i<(int)com_nodes.size(); i++) {
  //	strcallback = "/NodeList/" + std::to_string(com_nodes[i]) + "/ApplicationList/" + "*/SentData";
  //	Config::ConnectWithoutContext(strcallback, MakeCallback(&SentDataCallbackCom));
  //}

  //Trace received data at [physical nodes]
  //for (int i=0; i<(int)phy_nodes.size(); i++) {
  //	strcallback = "/NodeList/" + std::to_string(phy_nodes[i]) + "/ApplicationList/" + "*/ReceivedData";
  //	Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedDataCallbackPhy));
  //}

  //Trace received interest at [aggregation nodes]
  //for (int i=0; i<(int)agg_nodes.size(); i++) {
  //	strcallback = "/NodeList/" + std::to_string(agg_nodes[i]) + "/ApplicationList/" + "*/ReceivedInterest";
  //	Config::ConnectWithoutContext(strcallback, MakeCallback(&ReceivedInterestCallbackAgg));
  //}

  //Trace sent interest at [aggregation nodes]
  //for (int i=0; i<(int)agg_nodes.size(); i++) {
  //	strcallback = "/NodeList/" + std::to_string(agg_nodes[i]) + "/ApplicationList/" + "*/SentInterest";
  //	Config::ConnectWithoutContext(strcallback, MakeCallback(&SentInterestCallbackAgg));
  //}

  Simulator::Stop(Seconds(200.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

//Check if source/destination flow is permitted in config file (one-to-many, many-to-one flows)
bool FlowPermitted (int dstnode, int srcnode) {

	for (int i=0; i<(int)all_flows.size(); i++) {
		if((all_flows[i].first == dstnode) && (all_flows[i].second == srcnode)) {
			//Flow found
			return true;
		}
	}
	return false;
}

//Get source node ID from which payload interest came from
uint32_t GetSourceNodeID (std::string name) {

	//Find first occurence of "/" and get remaining string to its right
	std::size_t pos = name.find("/");
	std::string remstr = name.substr(pos+1,string::npos);

	//Find second occurence of "/" and get remaining string to its right
        pos = remstr.find("/");
        remstr = remstr.substr(pos+1,string::npos);

	//Find third occurence of "/" and get remaining string to its right
        pos = remstr.find("/");
        remstr = remstr.substr(pos+1,string::npos);

	//Find fourth occurence of "/" and get source node ID
        pos = remstr.find("/");
        remstr = remstr.substr(3,pos-3);

	return stoi(remstr);
}

 
//Split a string delimited by space
std::vector<std::string> SplitString(std::string strLine) {
        std::string str = strLine;
        std::vector<std::string> result;
        std::istringstream isstr(str);
        for(std::string str; isstr >> str; )
                result.push_back(str);

        return result;
}

//Store unique PMU IDs to prevent repeated installation of app
bool IsPMUAppInstalled(std::string PMUID) {
        for (int i=0; i<(int)uniquePMUs.size(); i++) {
                if(PMUID.compare(uniquePMUs[i]) == 0) {
                        return true;
                }
        }

        uniquePMUs.push_back(PMUID);
        return false;
}

//Store unique PDC IDs to prevent repeated installation of app
bool IsPDCAppInstalled(std::string PDCID) {
	for (int i=0; i<(int)uniquePDCs.size(); i++) {
		if(PDCID.compare(uniquePDCs[i]) == 0) {
			return true;
		}
	}

	uniquePDCs.push_back(PDCID);
	return false;
}

//Store unique WAC IDs to prevent repeated installation of app
bool IsWACAppInstalled(std::string WACID) {
        for (int i=0; i<(int)uniqueWACs.size(); i++) {
                if(WACID.compare(uniqueWACs[i]) == 0) {
                        return true;
                }
        }

        uniqueWACs.push_back(WACID);
        return false;
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
		std::stringstream sstr;
		sstr << interest->getName();
		std::string iname;
		sstr >> iname;

		//Only log flows that are permitted in config file (eliminates redundant interests received from multiple interfaces and to other nodes)
		if (FlowPermitted((int)nodeid, (int)GetSourceNodeID(iname)) == true) {
			tracefile << nodeid << ", recv, " << interest->getName() << ", " << interest->getPayloadLength() << ", " << std::fixed << setprecision(9)
                         << (Simulator::Now().GetNanoSeconds())/1000000000.0 << std::endl;
		}
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
