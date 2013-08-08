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
#include "ns3/service-flow.h"
#include <iostream>

#define MAX_USERS 1000
NS_LOG_COMPONENT_DEFINE ("WimaxSimpleExample");

using namespace ns3;

//uplink
UdpServerHelper udpServerUL[MAX_USERS];
ApplicationContainer serverAppsUL[MAX_USERS];
UdpClientHelper udpClientUL[MAX_USERS];
ApplicationContainer clientAppsUL[MAX_USERS];
IpcsClassifierRecord UlClassifierUgs[MAX_USERS];
ServiceFlow UlServiceFlowUgs[MAX_USERS];

//downlink
UdpServerHelper udpServerDL[MAX_USERS];
ApplicationContainer serverAppsDL[MAX_USERS];
UdpClientHelper udpClientDL[MAX_USERS];
ApplicationContainer clientAppsDL[MAX_USERS];
IpcsClassifierRecord DlClassifierUgs[MAX_USERS];
ServiceFlow DlServiceFlowUgs[MAX_USERS];

void NodeConfigurationCommonSetup(WimaxHelper &wimax, int numUsers, InternetStackHelper &stack, Ipv4AddressHelper &address, WimaxHelper::SchedulerType scheduler, /*inputs*/
		NodeContainer &ssNodes, Ptr<SubscriberStationNetDevice> *ss, Ipv4InterfaceContainer &SSinterfaces /*outputs*/)
{
	/*------------------------------*/
	//For each application, we need to create common variables here (common part between UL and DL of each application):
	//ssNodes = new NodeContainer;
	ssNodes.Create (numUsers);
	NS_LOG_UNCOND("@NodeConfigurationCommonSetup:" << numUsers << "==" << ssNodes.GetN ());
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

//TCP Traffic
int Uplink_TCP_Traffic(WimaxHelper &wimax, int port,int numUsers,int duration, Ipv4InterfaceContainer BSinterface, Ipv4InterfaceContainer SSinterfaces, int &created_nodes,
		NodeContainer bsNodes, NodeContainer ssNodes , Ptr<SubscriberStationNetDevice> ss[], std::string on_time, std::string off_time, int fid, ServiceFlow::SchedulingType  schedulinType,
		std::string data_rate, uint32_t pktSize)
{
	port+=10;
	//Collect an adjacency list of nodes for the p2p topology
	std::vector<NodeContainer> MonnodeAdjacencyList (numUsers);
	for(uint32_t i=0; i<MonnodeAdjacencyList.size (); ++i)
	{
		MonnodeAdjacencyList[i] = NodeContainer (bsNodes, ssNodes.Get (i));
	}

	OnOffHelper MonclientHelper ("ns3::TcpSocketFactory", Address ());
	MonclientHelper.SetAttribute
	("OnTime",  StringValue (on_time));//RandomVariableValue (ConstantVariable (1)));//on_time
	MonclientHelper.SetAttribute
	("OffTime", StringValue (off_time));
	MonclientHelper.SetAttribute("FlowId", UintegerValue (fid));
	MonclientHelper.SetConstantRate (DataRate (data_rate));
	MonclientHelper.SetAttribute ("PacketSize", UintegerValue (pktSize));

	Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
	PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
	ApplicationContainer sinkApp = sinkHelper.Install (bsNodes);
	sinkApp.Start (Seconds (1.0));
	sinkApp.Stop (Seconds (duration));

	PointToPointHelper p2p;
	std::vector<NetDeviceContainer> MondeviceAdjacencyList (numUsers);
	for(uint32_t i=0; i<MondeviceAdjacencyList.size (); ++i)
	{
	MondeviceAdjacencyList[i] = p2p.Install (MonnodeAdjacencyList[i]);
	}
	// Later, we add IP addresses.
	NS_LOG_INFO ("Assign IP Addresses.");
	Ipv4AddressHelper ipv4;

	std::vector<Ipv4InterfaceContainer> MoninterfaceAdjacencyList (numUsers);

	for( uint32_t i=0; i<MoninterfaceAdjacencyList.size (); ++i)
	{
		std::ostringstream subnet;
		int cc,dd;
		dd = ((i+1+created_nodes)%62) +1;  //first 6 digit of dd
		cc = (i+1+created_nodes-dd+1)/62 +  1; // created_nodes
		dd *= 4;
		subnet << "10.1."<<  cc << "." << dd ;
		NS_LOG_UNCOND("cn: " << created_nodes << ", i: " << i << ", cc: " << cc << ", dd:" << dd);
		ipv4.SetBase (subnet.str ().c_str (), "255.255.255.252");
		MoninterfaceAdjacencyList[i] = ipv4.Assign (MondeviceAdjacencyList[i]);
	}

	//normally wouldn't need a loop here but the server IP address is different
	//on each p2p subnet
	// ApplicationContainer MonclientApps;
	ApplicationContainer* MonclientApps = new ApplicationContainer[numUsers];
	for(uint32_t i=0; i< ssNodes.GetN (); ++i)
	{
		AddressValue remoteAddress
		(InetSocketAddress (MoninterfaceAdjacencyList[i].GetAddress (0), port));
		MonclientHelper.SetAttribute ("Remote", remoteAddress);
		// clientHelper.SetAttribute ("FlowId", UintegerValue (i));
		MonclientApps[i] = MonclientHelper.Install (ssNodes.Get (i));//      MonclientApps.Add
		IpcsClassifierRecord UlClassifier  = IpcsClassifierRecord(MoninterfaceAdjacencyList[i].GetAddress (1),//
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
		MonclientApps[i].Start (Seconds (1.0));  //t.GetValue()));
		MonclientApps[i].Stop (Seconds (duration));
	}


	created_nodes+= numUsers;
	return port;
}

//int Downlink_TCP_Traffic(WimaxHelper &wimax, int port,int numUsers,int duration, Ipv4InterfaceContainer BSinterface, Ipv4InterfaceContainer SSinterfaces, int &created_nodes,
//		NodeContainer bsNodes, NodeContainer ssNodes , Ptr<SubscriberStationNetDevice> ss[], std::string on_time, std::string off_time, int fid, ServiceFlow::SchedulingType  schedulinType,
//		std::string data_rate, uint32_t pktSize)
//{
//		port+=10;
//    	//Collect an adjacency list of nodes for the p2p topology
//    	std::vector<NodeContainer> MonnodeAdjacencyList (numUsers);
//    	for(uint32_t i=0; i<MonnodeAdjacencyList.size (); ++i)
//    	{
//    		MonnodeAdjacencyList[i] = NodeContainer (bsNodes, ssNodes.Get (i));
//    	}
//
//	  	  OnOffHelper MonclientHelper ("ns3::TcpSocketFactory", Address ());
//		  MonclientHelper.SetAttribute
//		    ("OnTime",  StringValue (on_time));//RandomVariableValue (ConstantVariable (1)));//on_time
//		  MonclientHelper.SetAttribute
//		    ("OffTime", StringValue (off_time));
//		  //MonclientHelper.SetAttribute("FlowId", UintegerValue (fid));
//		  MonclientHelper.SetConstantRate (DataRate (data_rate));
//		  MonclientHelper.SetAttribute ("PacketSize", UintegerValue (pktSize));
//
//		  PointToPointHelper p2p;
//		  std::vector<NetDeviceContainer> MondeviceAdjacencyList (numUsers);
//		  for(uint32_t i=0; i<MondeviceAdjacencyList.size (); ++i)
//		      {
//		        MondeviceAdjacencyList[i] = p2p.Install (MonnodeAdjacencyList[i]);
//		      }
//		   // Later, we add IP addresses.
//		 NS_LOG_INFO ("Assign IP Addresses.");
//		 Ipv4AddressHelper ipv4;
//
//		 std::vector<Ipv4InterfaceContainer> MoninterfaceAdjacencyList (numUsers);
//
//		 for( uint32_t i=0; i<MoninterfaceAdjacencyList.size (); ++i)
//		   {
//			 std::ostringstream subnet;
//			 int cc,dd;
//			 dd = ((i+1+created_nodes)%62) +1;  //first 6 digit of dd
//			 cc = (i+1+created_nodes-dd+1)/62 +  1; // created_nodes
//			 dd *= 4;
//			 subnet << "10.1."<<  cc << "." << dd ;
//			 NS_LOG_UNCOND("cn: " << created_nodes << ", i: " << i << ", cc: " << cc << ", dd:" << dd);
//			 ipv4.SetBase (subnet.str ().c_str (), "255.255.255.252");
//			 MoninterfaceAdjacencyList[i] = ipv4.Assign (MondeviceAdjacencyList[i]);
//		   }
//
//		  //normally wouldn't need a loop here but the server IP address is different
//		  //on each p2p subnet
//		 // ApplicationContainer MonclientApps;
//		  ApplicationContainer* MonclientApps = new ApplicationContainer[numUsers];
//		  for(uint32_t i=0; i< ssNodes.GetN (); ++i)
//		    {
//		      AddressValue remoteAddress
//		        (InetSocketAddress (MoninterfaceAdjacencyList[i].GetAddress (0), port));
//		      MonclientHelper.SetAttribute ("Remote", remoteAddress);
//		     // clientHelper.SetAttribute ("FlowId", UintegerValue (i));
//		      MonclientApps[i] = MonclientHelper.Install (ssNodes.Get (i));//      MonclientApps.Add
//		      IpcsClassifierRecord UlClassifier  = IpcsClassifierRecord(MoninterfaceAdjacencyList[i].GetAddress (1),//
//		       												Ipv4Mask ("255.255.255.252"),
//		       												Ipv4Address ("0.0.0.0"), //BSinterface.GetAddress (0),
//		       												Ipv4Mask ("0.0.0.0"),
//		       												0,
//		       												65000,
//		       												port,
//		       												port,
//		       												17,
//		       												1);
//			  ServiceFlow UlServiceFlowUgs = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_UP, schedulinType,UlClassifier);
//			  ss[i]->AddServiceFlow (UlServiceFlowUgs);
//			  //UniformVariable t;
//			  MonclientApps[i].Start (Seconds (1.0));  //t.GetValue()));
//			  MonclientApps[i].Stop (Seconds (duration));
//		    }
//
//
//		  created_nodes+= numUsers;
//
//		  return port;
//}

int Uplink_UDP_Traffic(WimaxHelper &wimax, int port,int numUsers,int duration, Ipv4InterfaceContainer BSinterface, Ipv4InterfaceContainer SSinterfaces,
		NodeContainer bsNodes, NodeContainer ssNodes , Ptr<SubscriberStationNetDevice> ss[])
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
		  udpClientUL[i].SetAttribute ("Interval", TimeValue (Seconds (1)));
		  udpClientUL[i].SetAttribute ("PacketSize", UintegerValue (128));
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
		udpClientDL[i].SetAttribute ("Interval", TimeValue (Seconds (1)));
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
															ServiceFlow::SF_TYPE_RTPS,
															DlClassifierUgs[i]);
		ss[i]->AddServiceFlow (DlServiceFlowUgs[i]);
	}

	return port;
}
int main (int argc, char *argv[])
{
	bool verbose = false;
	int duration = 7, schedType = 0, enableUplink = false, enableDownlink=false, port=100;
	int numPtctUsers=0,numMonUsers=0,numCtrlUsers=0,numSMUsers=0, created_nodes = 0;


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
	cmd.Parse (argc, argv);

	LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
	LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
	LogComponentEnable ("OnOffApplication", LOG_LEVEL_ALL); //TCP
	LogComponentEnable ("PacketSink", LOG_LEVEL_ALL); //TCP Sink


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

	if (verbose)
	{
	  wimax.EnableLogComponents ();  // Turn on all wimax logging
	}

	/*COMMON*/
	NodeContainer ssNodesPtct;
	Ptr<SubscriberStationNetDevice> ssPtct[numPtctUsers];
	Ipv4InterfaceContainer SSinterfacesPtct;
	if (numPtctUsers > 0)
	{
		NodeConfigurationCommonSetup(wimax,numPtctUsers,stack,address,scheduler,ssNodesPtct,ssPtct,SSinterfacesPtct);
	}

	NodeContainer ssNodesMon;
	Ptr<SubscriberStationNetDevice> ssMon[numMonUsers];
	Ipv4InterfaceContainer SSinterfacesMon;
	if (numMonUsers > 0)
	{

		NodeConfigurationCommonSetup(wimax,numMonUsers,stack,address,scheduler,ssNodesMon,ssMon,SSinterfacesMon);
	}
	/* END COMMON*/

	/*------------------------------*/
	if (enableUplink)
	{
		//NodeConfigurationCommonSetup(wimax,5,stack,address,scheduler,ssNodes,ss,SSinterfaces);

		//Protection is UDP:
		if (numPtctUsers > 0)
		{
			port = Uplink_UDP_Traffic(wimax, port, numPtctUsers, duration, BSinterface, SSinterfacesPtct, bsNodes, ssNodesPtct, ssPtct);
		}

		//Monitoring is TCP:
		if (numMonUsers > 0)
		{
			created_nodes = numPtctUsers+numMonUsers; //we add because some addresses are allocated but not used in TCP @ common (i.e. SSinterfacesMon is not used in function below).
			port = Uplink_TCP_Traffic(wimax, port, numMonUsers, duration, BSinterface, SSinterfacesMon, created_nodes, bsNodes, ssNodesMon, ssMon,
					"ns3::ConstantRandomVariable[Constant=1.0]", "ns3::ConstantRandomVariable[Constant=0.01]", 1, ServiceFlow::SF_TYPE_NRTPS, "200kb/s", 256);
		}

	}
	//NS_LOG_UNCOND("after UL");
//	if (enableDownlink)
//	{
//		if (numPtctUsers > 0)
//		{
//			port = Downlink_UDP_Traffic(wimax, port, numPtctUsers, duration, SSinterfacesPtct, bsNodes, ssNodesPtct, ssPtct);
//		}
//
//		//Monitoring is TCP:
//		if (numMonUsers > 0)
//		{
//			created_nodes = numPtctUsers;
//			port = Downlink_TCP_Traffic(wimax, port, numMonUsers, duration, BSinterface, SSinterfacesMon, created_nodes, bsNodes, ssNodesMon, ssMon,
//					"ns3::ConstantRandomVariable[Constant=1.0]", "ns3::ConstantRandomVariable[Constant=0.01]",  ServiceFlow::SF_TYPE_NRTPS, "200kb/s", 256);
//		}
//	}
//  if (enableUplink)
//  {
//	  //uplink
//	  UdpServerHelper udpServerUL[MAX_USERS];
//	  ApplicationContainer serverAppsUL[MAX_USERS];
//	  UdpClientHelper udpClientUL[MAX_USERS];
//	  ApplicationContainer clientAppsUL[MAX_USERS];
//	  IpcsClassifierRecord UlClassifierUgs[MAX_USERS];
//	  ServiceFlow UlServiceFlowUgs[MAX_USERS];
//
//	  for (int i = 0; i < numUsers; i++)
//	  {
//		  udpServerUL[i] = UdpServerHelper (100+10*i);
//		  serverAppsUL[i] = udpServerUL[i].Install (bsNodes.Get (0));
//		  serverAppsUL[i].Start (Seconds (0));
//		  serverAppsUL[i].Stop (Seconds (duration));
//		  udpClientUL[i] = UdpClientHelper (BSinterface.GetAddress (0), 100+10*i);
//		  udpClientUL[i].SetAttribute ("MaxPackets", UintegerValue (1200));
////		  NS_LOG_UNCOND("SSSS:" << Seconds (0.0066) << std::endl);
//		  udpClientUL[i].SetAttribute ("Interval", TimeValue (Seconds (1)));
//		  udpClientUL[i].SetAttribute ("PacketSize", UintegerValue (128));
//		  clientAppsUL[i] = udpClientUL[i].Install (ssNodes.Get(i));
//		  clientAppsUL[i].Start (Seconds (1.5));
//		  clientAppsUL[i].Stop (Seconds (duration));
//	  }
//
//	  //classifier
//	  for (int i = 0; i < numUsers; i++)
//	  {
//		  UlClassifierUgs[i] = IpcsClassifierRecord (SSinterfaces.GetAddress (i),
//	  											Ipv4Mask ("255.255.255.255"),
//	  											Ipv4Address ("0.0.0.0"),
//	  											Ipv4Mask ("0.0.0.0"),
//	  											0,
//	  											65000,
//	  											100+10*i,
//	  											100+10*i,
//	  											17,
//	  											1);
//	  	  UlServiceFlowUgs[i] = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_UP,
//	  															  ServiceFlow::SF_TYPE_UGS,
//	  															  UlClassifierUgs[i]);
//	  	  ss[i]->AddServiceFlow (UlServiceFlowUgs[i]);
//	  	}
//  }
//  if (enableDownlink)
//  {
//	  //downlink
//	  UdpServerHelper udpServerDL[MAX_USERS];
//	  ApplicationContainer serverAppsDL[MAX_USERS];
//	  UdpClientHelper udpClientDL[MAX_USERS];
//	  ApplicationContainer clientAppsDL[MAX_USERS];
//	  IpcsClassifierRecord DlClassifierUgs[MAX_USERS];
//	  ServiceFlow DlServiceFlowUgs[MAX_USERS];
//	  for (int i = 0; i < numUsers; i++)
//	  {
//		  //create applications whose servers are on ssNodes
//		  udpServerDL[i] = UdpServerHelper (1000+100*i);
//		  serverAppsDL[i] = udpServerDL[i].Install(ssNodes.Get(i));
//		  serverAppsDL[i].Start (Seconds (0));
//		  serverAppsDL[i].Stop (Seconds (duration));
//		  //create clients on BS for each application
//		  udpClientDL[i] = UdpClientHelper (SSinterfaces.GetAddress (i), 1000+100*i);
//		  udpClientDL[i].SetAttribute ("MaxPackets", UintegerValue (1200));
//		  udpClientDL[i].SetAttribute ("Interval", TimeValue (Seconds (1)));
//		  udpClientDL[i].SetAttribute ("PacketSize", UintegerValue (128));
//		  clientAppsDL[i] = udpClientDL[i].Install (bsNodes.Get(0));
//		  clientAppsDL[i].Start (Seconds (2));
//		  clientAppsDL[i].Stop (Seconds (duration));
//	  }
//
//	  //classifier
//	  for (int i = 0; i < numUsers; i++)
//	  {
//		DlClassifierUgs[i] = IpcsClassifierRecord  (Ipv4Address ("0.0.0.0"),
//											  Ipv4Mask ("0.0.0.0"),
//											  SSinterfaces.GetAddress (i),
//											  Ipv4Mask ("255.255.255.255"),
//											  0,
//											  65000,
//											  1000+100*i,
//											  1000+100*i,
//											  17,
//											  1);
//		  DlServiceFlowUgs[i] = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_DOWN,
//																ServiceFlow::SF_TYPE_UGS,
//																DlClassifierUgs[i]);
//		  ss[i]->AddServiceFlow (DlServiceFlowUgs[i]);
//	  }
//  }

  //run
  Simulator::Stop (Seconds (duration + 0.1));
  NS_LOG_INFO ("Starting simulation.....");
  Simulator::Run ();

//  //cleanup
//  for (int i = 0; i < numUsers; i++)
//  {
//	  (*ssPtr)[i] = 0;
//  }
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}
