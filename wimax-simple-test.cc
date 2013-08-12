/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *  Copyright (c) 2007,2008, 2009 INRIA, UDcast
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 *                              <amine.ismail@udcast.com>
 */

//
// Default network topology includes a base station (BS) and 2
// subscriber station (SS).

//      +-----+
//      | SS0 |
//      +-----+
//     10.1.1.1
//      -------
//        ((*))
//
//                  10.1.1.7
//               +------------+
//               |Base Station| ==((*))
//               +------------+
//
//        ((*))
//       -------
//      10.1.1.2
//       +-----+
//       | SS1 |
//       +-----+

// July 5: Modified for N users (SS) send to base station (BS). No Downlink yet.
//

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wimax-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipcs-classifier-record.h"
#include "assert.h"
#include "ns3/service-flow.h"
#include <iostream>

#define MAX_USERS 1205
NS_LOG_COMPONENT_DEFINE ("WimaxSimpleExample");


using namespace ns3;

//uplink udp:
UdpServerHelper udpServerUL[MAX_USERS];
ApplicationContainer serverAppsUL[MAX_USERS];
UdpClientHelper udpClientUL[MAX_USERS];
ApplicationContainer clientAppsUL[MAX_USERS];
IpcsClassifierRecord UlClassifierUgs[MAX_USERS];
ServiceFlow UlServiceFlowUgs[MAX_USERS];

//downlink udp:
UdpServerHelper udpServerDL[MAX_USERS];
ApplicationContainer serverAppsDL[MAX_USERS];
UdpClientHelper udpClientDL[MAX_USERS];
ApplicationContainer clientAppsDL[MAX_USERS];
IpcsClassifierRecord DlClassifierUgs[MAX_USERS];
ServiceFlow DlServiceFlowUgs[MAX_USERS];

void NodeConfigurationCommonSetupUDP(WimaxHelper &wimax, int numUsers, InternetStackHelper &stack, Ipv4AddressHelper &address, WimaxHelper::SchedulerType scheduler, /*inputs*/
		NodeContainer &ssNodes, Ptr<SubscriberStationNetDevice> *ss, Ipv4InterfaceContainer &SSinterfaces /*outputs*/)
{
	/*------------------------------*/
	//For each application, we need to create common variables here (common part between UL and DL of each application):
	//ssNodes = new NodeContainer;
	ssNodes.Create (numUsers);
	NetDeviceContainer ssDevs;
	ssDevs = wimax.Install (ssNodes,
						  WimaxHelper::DEVICE_TYPE_SUBSCRIBER_STATION,
						  WimaxHelper::SIMPLE_PHY_TYPE_OFDM,
						  scheduler);
	stack.Install (ssNodes);
	SSinterfaces = address.Assign (ssDevs);
	for (int i = 0; i < numUsers; i++)
	{
	  ss[i] = ssDevs.Get (i)->GetObject<SubscriberStationNetDevice> ();
	  ss[i]->SetModulationType (WimaxPhy::MODULATION_TYPE_QAM16_12);
	}
	//NS_LOG_UNCOND("SSinterfaces 0:" << SSinterfaces.GetAddress(0));
}

void NodeConfigurationCommonSetupTCP(WimaxHelper &wimax, int numUsers, int &created_nodes, InternetStackHelper &stack, Ipv4AddressHelper &address, WimaxHelper::SchedulerType scheduler,
		NetDeviceContainer bsDevs, NodeContainer bsNodes, /*inputs*/
		NodeContainer &ssNodes, Ptr<SubscriberStationNetDevice> ss[], std::vector<Ipv4InterfaceContainer> &interfaceAdjacencyList) //Ipv4InterfaceContainer &SSinterfaces /*outputs*/)
{
	ssNodes.Create (numUsers);
	NetDeviceContainer ssDevs = wimax.Install (ssNodes,WimaxHelper::DEVICE_TYPE_SUBSCRIBER_STATION,WimaxHelper::SIMPLE_PHY_TYPE_OFDM,scheduler);
	stack.Install (ssNodes);
	std::vector<NodeContainer> MonnodeAdjacencyList (numUsers);
	for(uint32_t i=0; i<MonnodeAdjacencyList.size (); ++i)
	{
		MonnodeAdjacencyList[i] = NodeContainer (bsNodes, ssNodes.Get (i));
	}

	PointToPointHelper p2p;
	std::vector<NetDeviceContainer> MondeviceAdjacencyList (numUsers);

	for(uint32_t i=0; i<MondeviceAdjacencyList.size (); ++i)
	{
//		MondeviceAdjacencyList[i].Add(bsDevs);
//		NetDeviceContainer dev = wimax.Install (ssNodes.Get(i),WimaxHelper::DEVICE_TYPE_SUBSCRIBER_STATION,WimaxHelper::SIMPLE_PHY_TYPE_OFDM,scheduler);
//		if (dev==NULL)
//			NS_LOG_UNCOND("dev is null");
	   // Ptr<SubscriberStationNetDevice>* ss = new Ptr<SubscriberStationNetDevice>[numUsers];
//		MondeviceAdjacencyList[i].Add(dev);
		MondeviceAdjacencyList[i] = p2p.Install (MonnodeAdjacencyList[i]);
		ss[i] =ssDevs.Get (i)->GetObject<SubscriberStationNetDevice> (); //dev.Get(0)->
		if (ss[i] == NULL)
			NS_LOG_UNCOND("ss[i] is null.");
		ss[i]->SetModulationType (WimaxPhy::MODULATION_TYPE_QAM16_12);
	}
	// Later, we add IP addresses.
	NS_LOG_UNCOND ("Assigning a subnet IP Address for TCP nodes " << created_nodes << " to " << created_nodes+numUsers-1 << "...");
	Ipv4AddressHelper ipv4;

	for( uint32_t i=0; i<interfaceAdjacencyList.size (); ++i)
	{
		NS_LOG_UNCOND("C#"<<i);
		std::ostringstream subnet;
		int cc,dd;
		dd = ((i+1+created_nodes)%62) +1;  //first 6 digit of dd
		cc = (i+1+created_nodes-dd+1)/62 +  1; // created_nodes
		dd *= 4;
		subnet << "10.1."<<  cc << "." << dd ;
		NS_LOG_UNCOND("cn: " << created_nodes << ", i: " << i << ", cc: " << cc << ", dd:" << dd);
		ipv4.SetBase (subnet.str ().c_str (), "255.255.255.252");
		interfaceAdjacencyList[i] = ipv4.Assign (MondeviceAdjacencyList[i]);
	}
	created_nodes = created_nodes + numUsers;
}


//TCP Traffic
int Uplink_TCP_Traffic(WimaxHelper &wimax, int port,int numUsers,int duration, Ipv4InterfaceContainer BSinterface, std::vector<Ipv4InterfaceContainer> interfaceAdjacencyList,
		NodeContainer bsNodes, NodeContainer ssNodes , Ptr<SubscriberStationNetDevice> ss[], std::string on_time, std::string off_time,
		int fid, ServiceFlow::SchedulingType  schedulinType,std::string data_rate, uint32_t pktSize)
{
	port+=10;
	//Collect an adjacency list of nodes for the p2p topology
//	std::vector<NodeContainer> MonnodeAdjacencyList (numUsers);
//	for(uint32_t i=0; i<MonnodeAdjacencyList.size (); ++i)
//	{
//		MonnodeAdjacencyList[i] = NodeContainer (bsNodes, ssNodes.Get (i));
//	}
	OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
	clientHelper.SetAttribute
	("OnTime",  StringValue (on_time));
	clientHelper.SetAttribute
	("OffTime", StringValue (off_time));//off_time
	clientHelper.SetAttribute("FlowId", UintegerValue (fid));
	//clientHelper.SetAttribute("MaxBytes",  UintegerValue (pktSize));
	clientHelper.SetConstantRate (DataRate (data_rate));
	clientHelper.SetAttribute ("PacketSize", UintegerValue (pktSize));

	Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
	PacketSinkHelper sinkHelperUL("ns3::TcpSocketFactory", sinkLocalAddress);
	ApplicationContainer sinkApp = sinkHelperUL.Install (bsNodes);

	sinkApp.Start (Seconds (1.0));
	sinkApp.Stop (Seconds (duration));


	//normally wouldn't need a loop here but the server IP address is different
	//on each p2p subnet
	// ApplicationContainer MonclientApps;
	ApplicationContainer MonclientApps; // = new ApplicationContainer[numUsers];
	for(uint32_t i=0; i< ssNodes.GetN (); ++i)
	{
		AddressValue remoteAddress (InetSocketAddress (interfaceAdjacencyList[i].GetAddress (0), port));
		clientHelper.SetAttribute ("Remote", remoteAddress);
		// clientHelper.SetAttribute ("FlowId", UintegerValue (i));
		MonclientApps.Add(clientHelper.Install (ssNodes.Get (i)));//      MonclientApps.Add
		IpcsClassifierRecord UlClassifier  = IpcsClassifierRecord(interfaceAdjacencyList[i].GetAddress (1),//
													Ipv4Mask ("255.255.255.252"),
													Ipv4Address ("0.0.0.0"), //BSinterface.GetAddress (0),
													Ipv4Mask ("0.0.0.0"),
													0,
													65000,
													port,
													port,
													17,
													1);
		ServiceFlow UlServiceFlowUgs = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_UP, schedulinType,UlClassifier);
		ss[i]->AddServiceFlow (UlServiceFlowUgs);
		//UniformVariable t;
	}
	MonclientApps.Start (Seconds (2.0));  //t.GetValue()));
	MonclientApps.Stop (Seconds (duration));
	return port;
}

int Downlink_TCP_Traffic(WimaxHelper &wimax, int port,int numUsers,int duration, Ipv4InterfaceContainer BSinterface, std::vector<Ipv4InterfaceContainer> interfaceAdjacencyList,
		NodeContainer bsNodes, NodeContainer ssNodes , Ptr<SubscriberStationNetDevice> ss[], std::string on_time, std::string off_time,
		int fid, ServiceFlow::SchedulingType  schedulinType,std::string data_rate, uint32_t pktSize)
{
	port+=10;
	//Collect an adjacency list of nodes for the p2p topology
//	std::vector<NodeContainer> MonnodeAdjacencyList (numUsers);
//	for(uint32_t i=0; i<MonnodeAdjacencyList.size (); ++i)
//	{
//		MonnodeAdjacencyList[i] = NodeContainer (bsNodes, ssNodes.Get (i));
//	}

	OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
	clientHelper.SetAttribute
	("OnTime",  StringValue (on_time));//RandomVariableValue (ConstantVariable (1)));//on_time
	clientHelper.SetAttribute
	("OffTime", StringValue (off_time));
	//clientHelper.SetAttribute("MaxBytes",  UintegerValue (pktSize));
	clientHelper.SetAttribute("FlowId", UintegerValue (fid));
	clientHelper.SetConstantRate (DataRate (data_rate));
	clientHelper.SetAttribute ("PacketSize", UintegerValue (pktSize));

	Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
	PacketSinkHelper sinkHelperDL("ns3::TcpSocketFactory", sinkLocalAddress);
	ApplicationContainer sinkApp = sinkHelperDL.Install (ssNodes);
	sinkApp.Start (Seconds (1.0));
	sinkApp.Stop (Seconds (duration));

	//normally wouldn't need a loop here but the server IP address is different
	//on each p2p subnet
	// ApplicationContainer MonclientApps;
	ApplicationContainer MonclientApps; // = new ApplicationContainer[numUsers];
	for(uint32_t i=0; i< ssNodes.GetN (); ++i)
	{
		AddressValue remoteAddress
		(InetSocketAddress (interfaceAdjacencyList[i].GetAddress (1), port));
		clientHelper.SetAttribute ("Remote", remoteAddress);
		// clientHelper.SetAttribute ("FlowId", UintegerValue (i));
		MonclientApps.Add(clientHelper.Install (bsNodes.Get (0)));//      MonclientApps.Add
		IpcsClassifierRecord DlClassifier  = IpcsClassifierRecord(Ipv4Address ("0.0.0.0"), // interfaceAdjacencyList[i].GetAddress(0)
													Ipv4Mask ("0.0.0.0"), // "255.255.255.252"
													interfaceAdjacencyList[i].GetAddress (1),
													Ipv4Mask ("255.255.255.252"),
													0,
													65000,
													port,
													port,
													17,
													1);
		ServiceFlow DlServiceFlow = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_DOWN, schedulinType,DlClassifier);
		ss[i]->AddServiceFlow (DlServiceFlow);
		//UniformVariable t;
	}
	MonclientApps.Start (Seconds (2.0));  //t.GetValue()));
	MonclientApps.Stop (Seconds (duration));
	return port;


}

int Uplink_UDP_Traffic(WimaxHelper &wimax, int port,int numUsers,int duration, Ipv4InterfaceContainer BSinterface, Ipv4InterfaceContainer SSinterfaces,
		NodeContainer bsNodes, NodeContainer ssNodes , Ptr<SubscriberStationNetDevice> ss[], int fid, uint32_t pktSize)
{
	int port1 = port;
	for (int i = 0; i < numUsers; i++)
	{
		  port = port+10;
		  udpServerUL[i] = UdpServerHelper (port);
		  serverAppsUL[i] = udpServerUL[i].Install (bsNodes.Get (0));
		  serverAppsUL[i].Start (Seconds (0));
		  serverAppsUL[i].Stop (Seconds (duration));
		  udpClientUL[i] = UdpClientHelper (BSinterface.GetAddress (0), port);
		  udpClientUL[i].SetAttribute ("MaxPackets", UintegerValue (1200));
		  udpClientUL[i].SetAttribute ("Interval", TimeValue (Seconds (2)));//0.01
		  udpClientUL[i].SetAttribute("FlowId", UintegerValue(fid));
		  udpClientUL[i].SetAttribute ("PacketSize", UintegerValue (pktSize));
		  clientAppsUL[i] = udpClientUL[i].Install (ssNodes.Get(i));
		  clientAppsUL[i].Start (Seconds (1.5));
		  clientAppsUL[i].Stop (Seconds (duration));
	}

	//classifier
	for (int i = 0; i < numUsers; i++)
	{
		  UlClassifierUgs[i] = IpcsClassifierRecord (SSinterfaces.GetAddress (i),
												Ipv4Mask ("255.255.255.255"),
												Ipv4Address ("0.0.0.0"),
												Ipv4Mask ("0.0.0.0"),
												0,
												65000,
												port1+10*i,
												port1+10*i,
												17,
												1);
		  UlServiceFlowUgs[i] = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_UP,
																  ServiceFlow::SF_TYPE_RTPS,
																  UlClassifierUgs[i]);
		  ss[i]->AddServiceFlow (UlServiceFlowUgs[i]);
	}

	return port;
}

int Downlink_UDP_Traffic(WimaxHelper &wimax, int port,int numUsers,int duration, Ipv4InterfaceContainer SSinterfaces,
		NodeContainer bsNodes, NodeContainer ssNodes , Ptr<SubscriberStationNetDevice> ss[])
{
	int port1 = port;
	for (int i = 0; i < numUsers; i++)
	{
		port=port+10;
		//create applications whose servers are on ssNodes
		udpServerDL[i] = UdpServerHelper (port);
		serverAppsDL[i] = udpServerDL[i].Install(ssNodes.Get(i));
		serverAppsDL[i].Start (Seconds (0));
		serverAppsDL[i].Stop (Seconds (duration));
		//create clients on BS for each application
		udpClientDL[i] = UdpClientHelper (SSinterfaces.GetAddress (i), port);
		udpClientDL[i].SetAttribute ("MaxPackets", UintegerValue (1200));
		udpClientDL[i].SetAttribute ("Interval", TimeValue (Seconds (2)));//0.01
		udpClientDL[i].SetAttribute("FlowId", UintegerValue(6));
		udpClientDL[i].SetAttribute ("PacketSize", UintegerValue (128));
		clientAppsDL[i] = udpClientDL[i].Install (bsNodes.Get(0));
		clientAppsDL[i].Start (Seconds (1.5));
		clientAppsDL[i].Stop (Seconds (duration));
	}

	//classifier
	for (int i = 0; i < numUsers; i++)
	{
		DlClassifierUgs[i] = IpcsClassifierRecord  (Ipv4Address ("0.0.0.0"),
										  Ipv4Mask ("0.0.0.0"),
										  SSinterfaces.GetAddress (i),
										  Ipv4Mask ("255.255.255.255"),
										  0,
										  65000,
										  port1+10*i,
										  port1+10*i,
										  17,
										  1);
		DlServiceFlowUgs[i] = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_DOWN,
															ServiceFlow::SF_TYPE_UGS,
															DlClassifierUgs[i]);
		ss[i]->AddServiceFlow (DlServiceFlowUgs[i]);
	}
	return port;
}
int main (int argc, char *argv[])
{
	bool verbose = false;
	int duration = 7, schedType = 0, enableUplink = false, enableDownlink=false, port=100, enableRural = false, enableSuburban = false, enableUrban = false;
	int numPtctUsers=0,numMonUsers=0,numCtrlUsers=0,numSMUsers=0, created_nodes = 0, density = 0;
	double areaRadius = 0.56434; //kilometer


	WimaxHelper::SchedulerType scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;

	CommandLine cmd;
	cmd.AddValue ("scheduler", "type of scheduler to use with the network devices", schedType);
	cmd.AddValue ("duration", "duration of the simulation in seconds", duration);
	cmd.AddValue ("verbose", "turn on all WimaxNetDevice log components", verbose);
	cmd.AddValue ("NP", "Number of nodes which have UL data.", numPtctUsers);
	cmd.AddValue ("NM", "Number of nodes which have UL data.", numMonUsers);
	cmd.AddValue ("NC", "Number of nodes which have UL data.", numCtrlUsers);
	cmd.AddValue ("NS", "Number of nodes which have UL data.", numSMUsers);
	cmd.AddValue ("DL", "Enable DL transmission?",enableDownlink);
	cmd.AddValue ("UL", "Enable UL transmission?",enableUplink);
	cmd.AddValue ("RL", "Enable Rural area transmission?",enableRural);
	cmd.AddValue ("SU", "Enable Suburban area transmission?",enableSuburban);
	cmd.AddValue ("U", "Enable Urban area transmission?",enableUrban);
	cmd.Parse (argc, argv);

	//LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
	//LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
	//LogComponentEnable ("OnOffApplication", LOG_LEVEL_ALL); //TCP
	//LogComponentEnable ("PacketSink", LOG_LEVEL_ALL); //TCP Sink
	//LogComponentEnable ("WimaxMacQueue", LOG_LEVEL_ALL);

	switch (schedType)
	{
	case 0:
	  scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;
	  break;
	case 1:
	  scheduler = WimaxHelper::SCHED_TYPE_MBQOS;
	  break;
	case 2:
	  scheduler = WimaxHelper::SCHED_TYPE_RTPS;
	  break;
	default:
	  scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;
	  break;
	}

	/*This is the field for defining common variables (the same for all applications)*/
	NodeContainer bsNodes;
	bsNodes.Create (1);
	WimaxHelper wimax;


	NetDeviceContainer bsDevs;
	bsDevs = wimax.Install (bsNodes, WimaxHelper::DEVICE_TYPE_BASE_STATION, WimaxHelper::SIMPLE_PHY_TYPE_OFDM, scheduler);
	Ptr<BaseStationNetDevice> bs;
	bs = bsDevs.Get (0)->GetObject<BaseStationNetDevice> ();

	InternetStackHelper stack;
	stack.Install (bsNodes);
	Ipv4AddressHelper address;
	address.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer BSinterface = address.Assign (bsDevs);

//	CsmaHelper csmaASN_BS;
//	CsmaHelper csmaASN_BS;
//	CsmaHelper csmaStreamer_ASN;
//
//	// First LAN BS and ASN
//	NodeContainer LAN_ASN_BS;
//
//	LAN_ASN_BS.Add (bsNodes.Get (0));
//
//
//
//	csmaASN_BS.SetChannelAttribute ("DataRate", DataRateValue (DataRate (10000000)));
//	csmaASN_BS.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
//	csmaASN_BS.SetDeviceAttribute ("Mtu", UintegerValue (1500));
//	NetDeviceContainer LAN_ASN_BS_Devs = csmaASN_BS.Install (LAN_ASN_BS);
//
//	NetDeviceContainer BS_CSMADevs;
//
//	BS_CSMADevs.Add (LAN_ASN_BS_Devs.Get (0));
//
//	NetDeviceContainer ASN_Devs1;
//	ASN_Devs1.Add (LAN_ASN_BS_Devs.Get (1));
//
//	// Second LAN ASN-GW and Streamer
//
//	NodeContainer LAN_ASN_STREAMER;
//	LAN_ASN_STREAMER.Add (ASNGW_Node.Get (0));
//	LAN_ASN_STREAMER.Add (Streamer_Node.Get (0));


	if (enableRural == true)
	{
		assert(enableUrban==false && enableSuburban==false);
		numMonUsers = 5;
		numCtrlUsers = 3;
		numPtctUsers = 1;
		density = 10;
		numSMUsers = int(3.14/3.0 * areaRadius * areaRadius * density);
	}
	if (enableSuburban == true)
	{
		assert(enableUrban==false && enableRural==false);
		numMonUsers = 11;
		numCtrlUsers = 8;
		numPtctUsers = 4;
		density = 800;
		numSMUsers = int(3.14/3.0 * areaRadius * areaRadius * density);
	}
	if (enableUrban == true)
	{
		assert(enableRural==false && enableSuburban==false);
		numMonUsers = 27;
		numCtrlUsers = 11;
		numPtctUsers = 8;
		density = 1200; //2000;
		numSMUsers = int(3.14/3.0 * areaRadius * areaRadius * density);
		assert(MAX_USERS>=numSMUsers);
	}



	if (verbose)
	{
	  wimax.EnableLogComponents ();  // Turn on all wimax logging
	}

	/*COMMON*/
	NS_LOG_UNCOND("Common UDP Protection Setup.");
	NodeContainer ssNodesPtct;
	Ptr<SubscriberStationNetDevice> ssPtct[numPtctUsers];
	Ipv4InterfaceContainer SSinterfacesPtct;
	if (numPtctUsers > 0)
	{
		NodeConfigurationCommonSetupUDP(wimax,numPtctUsers,stack,address,scheduler,ssNodesPtct,ssPtct,SSinterfacesPtct);
	}

	NS_LOG_UNCOND("Common UDP Monitoring (UL) Setup.");
	NodeContainer ssNodesMonUL;
	Ptr<SubscriberStationNetDevice> ssMonUL[numMonUsers];
	Ipv4InterfaceContainer SSinterfacesMonUL;
	if (numMonUsers > 0)
	{
		NodeConfigurationCommonSetupUDP(wimax,numMonUsers,stack,address,scheduler,ssNodesMonUL,ssMonUL,SSinterfacesMonUL);
	}


	NS_LOG_UNCOND("Common TCP Monitoring (DL) Setup.");
	NodeContainer ssNodesMonDL;
	Ptr<SubscriberStationNetDevice> ssMonDL[numMonUsers];
	std::vector<Ipv4InterfaceContainer> MonDLinterfaceAdjacencyList (numMonUsers);
	if (numMonUsers > 0)
	{
		created_nodes = numPtctUsers;
		NodeConfigurationCommonSetupTCP(wimax,numMonUsers,created_nodes,stack,address,scheduler,bsDevs,bsNodes,ssNodesMonDL,ssMonDL,MonDLinterfaceAdjacencyList);
	}

	NS_LOG_UNCOND("Common TCP Control Setup.");
	NodeContainer ssNodesCtrl;
	Ptr<SubscriberStationNetDevice> ssCtrl[numCtrlUsers];
	std::vector<Ipv4InterfaceContainer> CtrlinterfaceAdjacencyList (numCtrlUsers);
	if (numCtrlUsers > 0)
	{
		created_nodes = numPtctUsers+numMonUsers;
		NodeConfigurationCommonSetupTCP(wimax,numCtrlUsers,created_nodes,stack,address,scheduler,bsDevs,bsNodes,ssNodesCtrl,ssCtrl,CtrlinterfaceAdjacencyList);
	}

	NS_LOG_UNCOND("Common TCP Smart Meters Setup.");
	NodeContainer ssNodesSM;
	Ptr<SubscriberStationNetDevice> ssSM[numSMUsers];
	std::vector<Ipv4InterfaceContainer> SMinterfaceAdjacencyList (numSMUsers);
	if (numSMUsers > 0)
	{
		created_nodes = numPtctUsers+numMonUsers+numCtrlUsers;
		NodeConfigurationCommonSetupTCP(wimax,numSMUsers,created_nodes,stack,address,scheduler,bsDevs,bsNodes,ssNodesSM,ssSM,SMinterfaceAdjacencyList);
	}
	/* END COMMON*/

	/*------------------------------*/
	if (enableUplink)
	{
		//NodeConfigurationCommonSetup(wimax,5,stack,address,scheduler,ssNodes,ss,SSinterfaces);

		//Protection is UDP:
		if (numPtctUsers > 0)
		{
			NS_LOG_UNCOND("Setting up UL UDP for Protection.");
			port = Uplink_UDP_Traffic(wimax, port, numPtctUsers, duration, BSinterface, SSinterfacesPtct, bsNodes, ssNodesPtct, ssPtct, 2, 128);
		}

		//Monitoring UL is UDP:
		if (numMonUsers > 0)
		{
			NS_LOG_UNCOND("Setting up UL UDP for Monitoring.");
			port = Uplink_UDP_Traffic(wimax, port, numMonUsers, duration, BSinterface, SSinterfacesMonUL, bsNodes, ssNodesMonUL, ssMonUL, 0, 256);
		}

		//Control is TCP:
		if (numCtrlUsers > 0)
		{
			NS_LOG_UNCOND("Setting up UL TCP for control.");
			port = Uplink_TCP_Traffic(wimax, port, numCtrlUsers, duration, BSinterface, CtrlinterfaceAdjacencyList,  bsNodes, ssNodesCtrl, ssCtrl,
					"ns3::ExponentialRandomVariable[Mean=1]", "ns3::ExponentialRandomVariable[Mean=5]", 1, ServiceFlow::SF_TYPE_RTPS, "5kb/s", 128);
		}

		//(no uplink monitoring)

		//SmartMetering is TCP:
		if (numSMUsers > 0)
		{
			NS_LOG_UNCOND("Setting up UL TCP for smart metering.");
			port = Uplink_TCP_Traffic(wimax, port, numSMUsers, duration, BSinterface, SMinterfaceAdjacencyList, bsNodes, ssNodesSM, ssSM,
					"ns3::ConstantRandomVariable[Constant=0.1]", "ns3::ConstantRandomVariable[Constant=4]", 3, ServiceFlow::SF_TYPE_BE, "1kb/s", 128);
		}

	}

	if (enableDownlink)
	{
		if (numPtctUsers > 0)
		{
			NS_LOG_UNCOND("Setting up DL UDP for Protection.");
			port = Downlink_UDP_Traffic(wimax, port, numPtctUsers, duration, SSinterfacesPtct, bsNodes, ssNodesPtct, ssPtct);
		}

		//Control is TCP:
		if (numCtrlUsers > 0)
		{
			NS_LOG_UNCOND("Setting up DL TCP for control.");
			port = Downlink_TCP_Traffic(wimax, port, numCtrlUsers, duration, BSinterface, CtrlinterfaceAdjacencyList, bsNodes, ssNodesCtrl, ssCtrl,
					"ns3::ConstantRandomVariable[Constant=1]", "ns3::ConstantRandomVariable[Constant=5]", 5, ServiceFlow::SF_TYPE_RTPS, "2kb/s", 128); //ns3::ExponentialRandomVariable[Mean=1] ns3::ExponentialRandomVariable[Mean=5]
		}

		//Monitoring is TCP:
		if (numMonUsers > 0)
		{
			NS_LOG_UNCOND("Setting up DL TCP for monitoring.");
			port = Downlink_TCP_Traffic(wimax, port, numMonUsers, duration, BSinterface, MonDLinterfaceAdjacencyList, bsNodes, ssNodesMonDL, ssMonDL,
					"ns3::ConstantRandomVariable[Constant=1]", "ns3::ConstantRandomVariable[Constant=5]", 4, ServiceFlow::SF_TYPE_NRTPS, "10kb/s", 256);
		}

		//SmartMetering is TCP:
//		if (numSMUsers > 0)
//		{
//			NS_LOG_UNCOND("Setting up DL TCP for smart metering.");
//			port = Downlink_TCP_Traffic(wimax, port, numSMUsers, duration, BSinterface, SMinterfaceAdjacencyList, bsNodes, ssNodesSM, ssSM,
//					"ns3::ConstantRandomVariable[Constant=0.1]", "ns3::ConstantRandomVariable[Constant=4]", 7, ServiceFlow::SF_TYPE_BE, "50b/s", 128);
//		}
	}
  //run
  Simulator::Stop (Seconds (duration + 0.1));
  NS_LOG_UNCOND ("Starting simulation for " << numPtctUsers << " Protection Users, " << numCtrlUsers << " Control Users, " <<
		  numMonUsers << " Monitoring Users (DL only), and " << numSMUsers << " Smart Meters...");
  Simulator::Run ();


  /******************************GET STATISTICS**********************************************************/
  Ptr<Node> myNode = bsNodes.Get(0);
  NS_LOG_UNCOND("There are a total of " << bsNodes.Get(0)->GetNApplications() << " applications installed on the BS.");
  for (unsigned int appnum=4; appnum < bsNodes.Get(0)->GetNApplications(); appnum++)
  {
	  Ptr<OnOffApplication> bsApp = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(bsNodes.Get(0)->GetApplication(appnum))));
	  for (int ff=0;ff<FLOW_NUM;ff++)
	  	  NS_LOG_UNCOND("app#" << appnum << ",flow#" << ff << ":" << bsApp->m_Sent[ff]);
  }
  //first get the number of sent packets:
  //on-off for Control DL
  int totalSentCtrlDL[MAX_USERS];
  for (int appnum=1+numMonUsers+numCtrlUsers; appnum<1+numMonUsers+2*numCtrlUsers; appnum++)
  {
	  Ptr<OnOffApplication> bsAppCtrlDL = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(bsNodes.Get(0)->GetApplication(appnum))));
	  totalSentCtrlDL[appnum-1-numCtrlUsers-numMonUsers] = bsAppCtrlDL->m_Sent[5];
	  NS_LOG_UNCOND("totalSentCtrlDL[" << appnum-1-numMonUsers-numCtrlUsers << "," << appnum << "]=" <<  bsAppCtrlDL->m_Sent[5] );
  }

  //on-off for Monitoring DL
  int totalSentMonDL[MAX_USERS];
  for (int appnum=1+numMonUsers+2*numCtrlUsers; appnum<1+numMonUsers+2*numCtrlUsers+numMonUsers; appnum++)
  {
	  Ptr<OnOffApplication> bsAppMonDL = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(bsNodes.Get(0)->GetApplication(appnum))));
	  totalSentMonDL[appnum-1-numMonUsers-2*numCtrlUsers] = bsAppMonDL->m_Sent[4];
	  NS_LOG_UNCOND("totalSentMonDL[" << appnum-1-numMonUsers-2*numCtrlUsers << "," << appnum << "]=" <<  bsAppMonDL->m_Sent[4] );
  }

  //on-off for SM DL
//  int totalSentSMDL[MAX_USERS];
//  for (int appnum=numPtctUsers+numMonUsers+2*numCtrlUsers+numMonUsers; appnum<numPtctUsers+numMonUsers+2*numCtrlUsers+numMonUsers+numSMUsers; appnum++)
//  {
//	  Ptr<OnOffApplication> bsAppSMDL = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(bsNodes.Get(0)->GetApplication(appnum))));
//	  totalSentSMDL[appnum-numPtctUsers-numMonUsers-2*numCtrlUsers-numMonUsers] = bsAppSMDL->m_Sent[7];
//	  NS_LOG_UNCOND("totalSentSMDL[" << appnum-numPtctUsers-numMonUsers-2*numCtrlUsers-numMonUsers << "," << appnum << "]=" <<  bsAppSMDL->m_Sent[7] );
//  }

  //UDP for Protection UL (sum)
  int totalSentPtctUL = 0;
  for (int j=0; j < numPtctUsers; j++)
  {
	  Ptr<UdpClient> myApp = Ptr<UdpClient> (dynamic_cast<UdpClient *> (PeekPointer(ssNodesPtct.Get(j)->GetApplication(0))));
	  totalSentPtctUL +=  myApp->Sent[2];
  }
  NS_LOG_UNCOND("totalSentPtctUL (fid=2)=" << totalSentPtctUL);
  //UDP for Monitoring UL (sum)
  int totalSentMonUL = 0;
  for (int j=0; j < numMonUsers; j++)
  {
	  Ptr<UdpClient> myApp = Ptr<UdpClient> (dynamic_cast<UdpClient *> (PeekPointer(ssNodesMonUL.Get(j)->GetApplication(0))));
	  totalSentMonUL +=  myApp->Sent[0];
  }
  NS_LOG_UNCOND("totalSentMonUL (fid=0)=" << totalSentMonUL);
  //on-off for Control UL (sum)
  int totalSentCtrlUL = 0;
  for (int j=0; j < numCtrlUsers; j++)
  {
	  Ptr<OnOffApplication> myApp = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(ssNodesCtrl.Get(j)->GetApplication(0))));
	  totalSentCtrlUL += myApp->m_Sent[1];
  }
  //on-off for SM UL (sum)
  int totalSentSMUL = 0;
  for (int j=0; j < numSMUsers; j++)
  {
	  Ptr<OnOffApplication> myApp = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(ssNodesSM.Get(j)->GetApplication(0))));
	  totalSentSMUL += myApp->m_Sent[3];
  }

  NS_LOG_UNCOND("TCP UL:\nFlow Id\tSent\tRecv\tReliable Received\tAvg. Delay\n");
  for (int i=0; i< FLOW_NUM; i++)
  {
	  int totalSent;
	  if (i==1)
		  totalSent = totalSentCtrlUL;
	  else if (i==3)
		  totalSent= totalSentSMUL;
	  else
		  continue;
	  NS_LOG_UNCOND(i << "\t" << totalSent << "\t" << myNode->Recv[i] << "\t" <<  myNode->reliable_received[i] << "\t\t"
			  << myNode->Delays[i].ToDouble(Time::MS)/float(myNode->Recv[i]) << std::endl );
  }


  NS_LOG_UNCOND("TCP DL Ctrl:\nFlow Id\tSent\tRecv\tReliable Received\tAvg. Delay\n");
  for (int j=0; j<numCtrlUsers; j++)
  {

	  NS_LOG_UNCOND("Control Node#" << j << "--");
	  Ptr<Node> myNode = ssNodesCtrl.Get(j);
	  Ptr<OnOffApplication> myApp = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(myNode->GetApplication(0))));
	  for (int i=0; i< FLOW_NUM; i++)
	  {
		  if (i==5)
		  NS_LOG_UNCOND(i << "\t" << totalSentCtrlDL[j] << "\t" << myNode->Recv[i] << "\t" <<  myNode->reliable_received[i] << "\t\t"
				  << myNode->Delays[i].ToDouble(Time::MS)/float(myNode->Recv[i]) << std::endl );

	  }
  }

  NS_LOG_UNCOND("TCP DL Mon:\nFlow Id\tSent\tRecv\tReliable Received\tAvg. Delay\n");
   for (int j=0; j<numMonUsers; j++)
   {
 	  NS_LOG_UNCOND("Monitoring Node#" << j << "--");
 	  Ptr<Node> myNode = ssNodesMonDL.Get(j);
 	  Ptr<OnOffApplication> myApp = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(myNode->GetApplication(0))));
 	  for (int i=0; i< FLOW_NUM; i++)
 	  {
 		  if (i==4)
 		  NS_LOG_UNCOND(i << "\t" << totalSentMonDL[j] << "\t" << myNode->Recv[i] << "\t" <<  myNode->reliable_received[i] << "\t\t"
 				  << myNode->Delays[i].ToDouble(Time::MS)/float(myNode->Recv[i]) << std::endl );

 	  }
   }

//  NS_LOG_UNCOND("TCP DL SM:\nFlow Id\tSent\tRecv\tReliable Received\tAvg. Delay\n");
//   for (int j=0; j<numSMUsers; j++)
//   {
// 	  NS_LOG_UNCOND("Smart Metering Node#" << j << "--");
// 	  Ptr<Node> myNode = ssNodesSM.Get(j);
// 	  Ptr<OnOffApplication> myApp = Ptr<OnOffApplication> (dynamic_cast<OnOffApplication *> (PeekPointer(myNode->GetApplication(0))));
// 	  for (int i=0; i< FLOW_NUM; i++)
// 	  {
// 		  if (i==7)
// 		  NS_LOG_UNCOND(i << "\t" << totalSentSMDL[j] << "\t" << myNode->Recv[i] << "\t" <<  myNode->reliable_received[i] << "\t\t"
// 				  << myNode->Delays[i].ToDouble(Time::MS)/float(myNode->Recv[i]) << std::endl );
//
// 	  }
//    }

//   NS_LOG_UNCOND("UDP Protection:\nFlow Id\tSent\tRecv\tReliable Received\tAvg. Delay\n");
//   for (int j=0; j<numPtctUsers; j++)
//   {
//	   NS_LOG_UNCOND("\n--Protection Node#" << j << "--\n");
//	   for (int i=0; i< FLOW_NUM; i++)
//	   {
//		   //udpServerUL[j].GetServer()->
//		   if (i==2)
//		   {
//			  NS_LOG_UNCOND (i << "\t" << /*Sent[i] <<*/ "\t" << udpServerUL[i].GetServer()->Recv[i] << "\t" <<  udpServerUL[j].GetServer()->reliable_received[i] << "\t\t"
//					   << udpServerUL[j].GetServer()->Delays[i].ToDouble(Time::MS)/float(udpServerUL[j].GetServer()->Recv[i]) << std::endl ) ;
//		   }
//	   }
//   }

//  //log: table summary
//  NS_LOG_UNCOND("Flow Id\tSent\tRecv\tReliable Received\tAvg. Delay\n");
//  for (int i=0; i< FLOW_NUM; i++)
//  {
//	 //if (Recv[i] != 0)
//
//	 udpServerUL[i].GetServer()->Delays
//	NS_LOG_UNCOND (i << "\t" << udpClientUL[i].Sent[i] << "\t" << udpServerUL[i].    .Recv[i] << "\t" <<  udpServerUL.reliable_received[i] << "\t\t"
//  << udpServerUL.Delays[i].ToDouble(Time::MS)/float(udpServerUL.Recv[i]) << std::endl ) ;
//  }

//  //cleanup
//  for (int i = 0; i < numUsers; i++)
//  {
//	  (*ssPtr)[i] = 0;
//  }
  Simulator::Destroy ();
  NS_LOG_UNCOND ("Done.");

  return 0;
}
