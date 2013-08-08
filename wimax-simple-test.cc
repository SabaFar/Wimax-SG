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
	//		  NS_LOG_UNCOND("SSSS:" << Seconds (0.0066) << std::endl);
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
	int duration = 7, schedType = 0, numUsers=1, enableUplink = false, enableDownlink=false, port=100;

	WimaxHelper::SchedulerType scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;

	CommandLine cmd;
	cmd.AddValue ("scheduler", "type of scheduler to use with the network devices", schedType);
	cmd.AddValue ("duration", "duration of the simulation in seconds", duration);
	cmd.AddValue ("verbose", "turn on all WimaxNetDevice log components", verbose);
	cmd.AddValue ("N", "Number of nodes which have UL data.", numUsers);
	cmd.AddValue ("DL", "Enable DL transmission?",enableDownlink);
	cmd.AddValue ("UL", "Enable UL transmission?",enableUplink);
	cmd.Parse (argc, argv);

	LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
	LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

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
	NodeContainer ssNodes;
	Ptr<SubscriberStationNetDevice> ss[numUsers];
	Ipv4InterfaceContainer SSinterfaces;
//	NS_LOG_UNCOND("Before COmmon");
	//NodeConfigurationCommonSetup(WimaxHelper &wimax, int numUsers, InternetStackHelper *stack, Ipv4AddressHelper *address, WimaxHelper::SchedulerType scheduler, /*inputs*/
	//		NodeContainer &ssNodes, Ptr<SubscriberStationNetDevice> *ss, Ipv4InterfaceContainer &SSinterfaces /*outputs*/)

	NodeConfigurationCommonSetup(wimax,numUsers,stack,address,scheduler,ssNodes,ss,SSinterfaces);
//	NS_LOG_UNCOND("After COmmon");
//	if (ssNodes==NULL)
//		NS_LOG_UNCOND("ssNodes is NULL");
//	if (ss == NULL)
//		NS_LOG_UNCOND("ss is NULL");
//	if (SSinterfaces==NULL)
//		NS_LOG_UNCOND("SSinterfaces is NULL");
	/*------------------------------*/
	if (enableUplink)
	{
		port = Uplink_UDP_Traffic(wimax, port, numUsers, duration, BSinterface, SSinterfaces, bsNodes, ssNodes, ss);
	}
	//NS_LOG_UNCOND("after UL");
	if (enableDownlink)
	{
		port = Downlink_UDP_Traffic(wimax, port, numUsers, duration, SSinterfaces, bsNodes, ssNodes, ss);
	}
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
