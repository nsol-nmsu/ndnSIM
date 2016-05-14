#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

#include <iostream>
#include <fstream>

namespace ns3 {

/**
 * This scenario simulates network graph generated by TopoMux (NMSU NSOL):
 *
 *     NS_LOG=ndn.Subscriber::ndn.Aggregator::ndn.SpontaneousProducer ./waf --run ndn-icens-simulation
 */


bool ValidatePrefix(int, std::string, std::string);
bool PrefixAdded(std::string, std::string);

// Vectors to store the various node types
vector<int> com_nodes, agg_nodes, phy_nodes;
// Vector to store distinct prefixes being served by the node types
vector<string> com_prefixes, agg_prefixes;

int
main(int argc, char* argv[])
{

  //--- Count the number of nodes to create
  ifstream nfile ("src/ndnSIM/examples/icens-nodes.txt", std::ios::in);
  //ifstream nfile ("src/ndnSIM/examples/a-nodes.txt", std::ios::in);
  std::string nodeid, nodename, nodetype;
  int nodecount = 0; //number of nodes in topology
  int numOfPMUs = 20; //number of PMUs

  if (nfile.is_open ()) {
  	while (nfile >> nodeid >> nodename >> nodetype) {
	//while(std::getline(nfile,line)){
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
  }
  nfile.close();

  // setting default parameters for PointToPoint links and channels
  //Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1000Mbps"));
  //Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("2ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Creating the number of nodes counted in the nodes file
  NodeContainer nodes;
  nodes.Create(nodecount);

  // Connecting nodes using two links
  PointToPointHelper p2p;

  //--- Get the edges of the graph from file and connect them
  ifstream efile ("src/ndnSIM/examples/icens-edges.txt", std::ios::in);
  //ifstream efile ("src/ndnSIM/examples/a-edges.txt", std::ios::in);
  std::string srcnode, dstnode, bw, delay, edgetype;

  if (efile.is_open ()) {
        while (efile >> srcnode >> dstnode >> bw >> delay >> edgetype) {
		//Set delay and bandwidth parameters for point-to-point links
		p2p.SetDeviceAttribute("DataRate", StringValue(bw+"Mbps"));
		p2p.SetChannelAttribute("Delay", StringValue(delay+"ms"));
		p2p.Install(nodes.Get(std::stoi(srcnode)), nodes.Get(std::stoi(dstnode)));
        }
  }
  else {
        std::cout << "Error::Cannot open edges file!!!" << std::endl;
  }
  efile.close();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.InstallAll();

  //--- Configure manual/static routes on all nodes
  //--- Install spontaneous producer application for each prefix that a node serves
  ifstream rfile("src/ndnSIM/examples/icens-routing-tables.txt", std::ios::in);
  //ifstream rfile("src/ndnSIM/examples/a-routing.txt", std::ios::in);
  Ptr<Node> currentnode, nexthopnode;
  std::string strfrom, prefixtoroute, strnexthop, strmetric; int metric;


  if (rfile.is_open ()) {
	// Spontaneuos producer helper
	ndn::AppHelper spHelper("ns3::ndn::SpontaneousProducer");
	ndn::AppHelper aggHelper("ns3::ndn::Aggregator");

        while (rfile >> strfrom >> prefixtoroute >> strnexthop >> strmetric) {

		if (strnexthop == "local") {

			if (prefixtoroute.find("agg") != std::string::npos) {
				if (prefixtoroute.find("overlay") != std::string::npos) {
					// Install aggregator app for AMI payload aggregation - "overlay"
  					aggHelper.SetPrefix(prefixtoroute + "/ami");
 	 				aggHelper.SetAttribute("UpstreamPrefix", StringValue(prefixtoroute.substr(0,prefixtoroute.find("agg"))+ "com/ami")); //forward to com node prefix
  					aggHelper.SetAttribute("Frequency",  StringValue("1")); //wait seconds before aggregatiion
  					aggHelper.Install(nodes.Get(std::stoi(strfrom)));

					// Install aggregator app for PMU payload aggregation - "overlay"
                                	aggHelper.SetPrefix(prefixtoroute + "/pmu");
                                	aggHelper.SetAttribute("UpstreamPrefix", StringValue(prefixtoroute.substr(0,prefixtoroute.find("agg"))+ "com/pmu")); //forward to com node prefix
                                	aggHelper.SetAttribute("Frequency",  StringValue("0.1")); ////wait seconds before aggregatiion
                                	aggHelper.Install(nodes.Get(std::stoi(strfrom)));
				}
				else {
					// serves /direct/agg
				}
			}
			else if (prefixtoroute.find("com") != std::string::npos){
				// Install spontaneous producer on the node for subscription and payload interests served by "direct"
				if (prefixtoroute.find("direct") != std::string::npos) {
                         		spHelper.SetPrefix(prefixtoroute + "/subscription");
                         		spHelper.SetAttribute("Frequency", StringValue("60")); //wait x seconds and send data for subscriptions
                         		spHelper.SetAttribute("PayloadSize", StringValue("1024"));
                         		spHelper.Install(nodes.Get(std::stoi(strfrom)));

					// Install spontaneous producer for error reporting with payload interest
					spHelper.SetPrefix(prefixtoroute + "/error");
                                        spHelper.SetAttribute("Frequency", StringValue("0"));
                                        spHelper.SetAttribute("PayloadSize", StringValue("1024"));
                                        spHelper.Install(nodes.Get(std::stoi(strfrom)));

				}
				else {
					// Install a spontaneous producer for aggregated AMI "overlay"
					spHelper.SetPrefix(prefixtoroute + "/ami");
                                	spHelper.SetAttribute("Frequency", StringValue("0")); //how many seconds to wait before sending data
                                	spHelper.SetAttribute("PayloadSize", StringValue("1024"));
                                	spHelper.Install(nodes.Get(std::stoi(strfrom)));

					// Install a spontaneous producer for aggregated PMU "overlay"
                                        spHelper.SetPrefix(prefixtoroute + "/pmu");
                                        spHelper.SetAttribute("Frequency", StringValue("0")); //how many seconds to wait before sending data
                                        spHelper.SetAttribute("PayloadSize", StringValue("1024"));
                                        spHelper.Install(nodes.Get(std::stoi(strfrom)));
				}

			}
			else {
				//physical node
			}

			// Store the prefixes served by compute nodes, will be used by physical nodes for subscription requests
			if (ns3::ValidatePrefix(std::stoi(strfrom), prefixtoroute, "com_") == true) {
				com_prefixes.push_back(prefixtoroute);
			}

			// Store the prefixes served by agg nodes, will be used aggregate interest payloads at aggregation layer
                        if (ns3::ValidatePrefix(std::stoi(strfrom), prefixtoroute, "agg_") == true) {
                                agg_prefixes.push_back(prefixtoroute);
                        }

		}
		else {
			// Configure static route on node
			currentnode = nodes.Get(std::stoi(strfrom)); // node to add route on
			prefixtoroute = prefixtoroute; // prefix to add route for
			nexthopnode = nodes.Get(std::stoi(strnexthop));     // next hop node
			metric = std::stoi(strmetric);     // metric or cost
			ndn::FibHelper::AddRoute(currentnode, prefixtoroute, nexthopnode, metric);
		}
        }
  }
  else {
        std::cout << "Error::Cannot open routing table file!!!" << std::endl;
  }
  rfile.close();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");

  // Installing applications

  // Subscriber
  ndn::AppHelper consumerHelper("ns3::ndn::Subscriber");
  // Subscriber send out subscription interest for a prefix...

  // Each physical layer node, subscribes to "direct" prefix being served by computes nodes
  for (int i=0; i<(int)com_prefixes.size(); i++) {
	//Subscribe to direct prefix
	if(com_prefixes[i].find("direct") != std::string::npos) {
  		for (int j=0; j<(int)phy_nodes.size(); j++) {
			//PMUs should not subscribe to data
			if ( j >= numOfPMUs) {
				consumerHelper.SetPrefix(com_prefixes[i] + "/subscription");
				consumerHelper.SetAttribute("TxTimer", StringValue("300")); //resend subscription interest every x seconds
				consumerHelper.SetAttribute("Subscription", IntegerValue(1)); //set the subscription value
  				consumerHelper.Install(nodes.Get(phy_nodes[j]));
			}
  		}
	}
  }

  // Each physical layer node sends payload interests to aggregation layer - "overlay"
  for (int i=0; i<(int)agg_prefixes.size(); i++) {
  	//Subscribe to overlay prefix
        if(agg_prefixes[i].find("overlay") != std::string::npos) {
  		for (int j=0; j<(int)phy_nodes.size(); j++) {

			if ( j < numOfPMUs) {
				 //PMU messages - hihger send rate
                                consumerHelper.SetPrefix(agg_prefixes[i] + "/pmu/phy" + std::to_string(phy_nodes[j]));
                                consumerHelper.SetAttribute("TxTimer", StringValue("0.1")); //resend payload interest every x seconds
                                consumerHelper.SetAttribute("Subscription", IntegerValue(0)); //payload interest (0 value)
                                consumerHelper.SetAttribute("PayloadSize", StringValue("9")); //payload size in bytes
				consumerHelper.SetAttribute("RetransmitPackets", IntegerValue(0)); //1 for retransmit, any other value does not retransmit
                                consumerHelper.Install(nodes.Get(phy_nodes[j]));
			}
			else {

				//For smart metering - AMI
  				consumerHelper.SetPrefix(agg_prefixes[i] + "/ami/phy" + std::to_string(phy_nodes[j]));
  				consumerHelper.SetAttribute("TxTimer", StringValue("1")); //resend payload interest every x seconds
  				consumerHelper.SetAttribute("Subscription", IntegerValue(0)); //payload interest (0 value)
  				consumerHelper.SetAttribute("PayloadSize", StringValue("7")); //payload size in bytes
  				consumerHelper.Install(nodes.Get(phy_nodes[j]));
			}
		}
	}
  }

  // Each physical layer node, sends payload interest to compute node for error reporting using "direct" prefix
  for (int i=0; i<(int)com_prefixes.size(); i++) {
        //Subscribe to direct prefix
        if(com_prefixes[i].find("direct") != std::string::npos) {
                for (int j=0; j<(int)phy_nodes.size(); j++) {
                       	consumerHelper.SetPrefix(com_prefixes[i] + "/error/phy" + std::to_string(phy_nodes[j]));
                       	consumerHelper.SetAttribute("TxTimer", StringValue("120")); //resend subscription interest every x seconds
                       	consumerHelper.SetAttribute("Subscription", IntegerValue(0)); //payload interest (0 value)
			consumerHelper.SetAttribute("PayloadSize", StringValue("6")); //payload size in bytes
                       	consumerHelper.Install(nodes.Get(phy_nodes[j]));
                }
        }
  }

  Simulator::Stop(Seconds(3600.0));

  //ndn::AppDelayTracer::InstallAll("icens-simulation-trace.txt");

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}


// Get the prefixes served the nodes at the various layers
bool ValidatePrefix(int nodeid, std::string prefix, std::string nodetype) {
        // verify if node id belongs to the node type passed to function
        if (nodetype == "com_") {
                for (int i=0; i<(int)com_nodes.size(); i++) {
                        if (com_nodes[i] == nodeid) {
				if (PrefixAdded(prefix, nodetype) == false) {
                                	return true;
				}
                        }
                }
        }
	else if (nodetype == "agg_") {
		for (int i=0; i<(int)agg_nodes.size(); i++) {
                        if (agg_nodes[i] == nodeid) {
                                if (PrefixAdded(prefix, nodetype) == false) {
                                        return true;
                                }
                        }
                }
	}
        else if (nodetype == "phy_") {

        }

        return false;
}

bool PrefixAdded(std::string prefix, std::string nodetype) {

	if (nodetype == "com_") {
		// check if prefix is already detected as being served by compute node
		for (int i=0; i<(int)com_prefixes.size(); i++) {
			if (prefix == com_prefixes[i]) {
				return true;
			}
		}
	}
	else if (nodetype == "agg_") {
		// check if prefix is already detected as being served by aggregation node
                for (int i=0; i<(int)agg_prefixes.size(); i++) {
                        if (prefix == agg_prefixes[i]) {
                                return true;
                        }
                }
	}
	else if (nodetype == "phy_") {

        }

	return false;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
