/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *  Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "packet-loss-counter.h"

#include "seq-ts-header.h"
#include "udp-server.h"
#include "ns3/flow-id-tag.h"

//#include "ns3/wimax-urban.h"
//#include "../src/wimax/examples/wimax-urban.h"

//#define FLOW_NUM 10
//int reliable_received[FLOW_NUM];
//ns3::Time latency[FLOW_NUM];
//ns3::Time Delays[FLOW_NUM];
//int Recv[FLOW_NUM];

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("UdpServer");
NS_OBJECT_ENSURE_REGISTERED (UdpServer);


TypeId
UdpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::UdpServer")
    .SetParent<Application> ()
    .AddConstructor<UdpServer> ()
    .AddAttribute ("Port",
                   "Port on which we listen for incoming packets.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&UdpServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketWindowSize",
                   "The size of the window used to compute the packet loss. This value should be a multiple of 8.",
                   UintegerValue (32),
                   MakeUintegerAccessor (&UdpServer::GetPacketWindowSize,
                                         &UdpServer::SetPacketWindowSize),
                   MakeUintegerChecker<uint16_t> (8,256))
  ;
  return tid;
}

UdpServer::UdpServer ()
  : m_lossCounter (0)

{
//  reliable_received[0]=0;
//  reliable_received[1]=0;
//  reliable_received[2]=0;
//  reliable_received[3]=0;
//  reliable_received[4]=0;
//  reliable_received[5]=0;
//  reliable_received[6]=0;
//  reliable_received[7]=0;
//  reliable_received[8]=0;
//  reliable_received[9]=0;
//
//  latency[0]= ns3::Time(ns3::Seconds(0.1));
//  latency[1]= ns3::Time(ns3::Seconds(0.1));
//  latency[2]= ns3::Time(ns3::Seconds(0.02));
//  latency[3]= ns3::Time(ns3::Seconds(5));
//  latency[4]= ns3::Time(ns3::Seconds(0.1));
//  latency[5]= ns3::Time(ns3::Seconds(0.1));
//  latency[6]= ns3::Time(ns3::Seconds(0.02));
//  latency[7]= ns3::Time(ns3::Seconds(5));
//  latency[8]= ns3::Time(ns3::Seconds(0.0));
//  latency[9]= ns3::Time(ns3::Seconds(0.0));
//
//  Delays[0]= ns3::Time(ns3::Seconds(0.0));
//  Delays[1]= ns3::Time(ns3::Seconds(0.0));
//  Delays[2]= ns3::Time(ns3::Seconds(0.0));
//  Delays[3]= ns3::Time(ns3::Seconds(0.0));
//  Delays[4]= ns3::Time(ns3::Seconds(0.0));
//  Delays[5]= ns3::Time(ns3::Seconds(0.0));
//  Delays[6]= ns3::Time(ns3::Seconds(0.0));
//  Delays[7]= ns3::Time(ns3::Seconds(0.0));
//  Delays[8]= ns3::Time(ns3::Seconds(0.0));
//  Delays[9]= ns3::Time(ns3::Seconds(0.0));
//
//  Recv[0]=0;
//  Recv[1]=0;
//  Recv[2]=0;
//  Recv[3]=0;
//  Recv[4]=0;
//  Recv[5]=0;
//  Recv[6]=0;
//  Recv[7]=0;
//  Recv[8]=0;
//  Recv[9]=0;

  NS_LOG_FUNCTION (this);
  m_received=0;
}

UdpServer::~UdpServer ()
{
  NS_LOG_FUNCTION (this);
}

uint16_t
UdpServer::GetPacketWindowSize () const
{
  NS_LOG_FUNCTION (this);
  return m_lossCounter.GetBitMapSize ();
}

void
UdpServer::SetPacketWindowSize (uint16_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_lossCounter.SetBitMapSize (size);
}

uint32_t
UdpServer::GetLost (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lossCounter.GetLost ();
}

uint32_t
UdpServer::GetReceived (void) const
{
  NS_LOG_FUNCTION (this);
  return m_received;
}

void
UdpServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
UdpServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                                   m_port);
      m_socket->Bind (local);
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (),
                                                   m_port);
      m_socket6->Bind (local);
    }

  m_socket6->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));

}

void
UdpServer::StopApplication ()
{
  //NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
UdpServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;

  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
//          SeqTsHeader seqTs;
//          packet->RemoveHeader (seqTs);
//          uint32_t currentSequenceNumber = seqTs.GetSeq ();
    	  SeqTsHeader seqTs;
		  packet->RemoveHeader (seqTs);
		  uint32_t currentSequenceNumber = seqTs.GetSeq ();
		  Time current_Delay = Time(0);
		  FlowIdTag myflowid;
		  packet->FindFirstMatchingByteTag (myflowid);  //see http://www.nsnam.org/doxygen-release/wifi-phy-test_8cc_source.html

		  current_Delay = Simulator::Now () - seqTs.GetTs();
		  this->GetNode()->Delays[myflowid.GetFlowId()] += current_Delay;
		  this->GetNode()->Recv[myflowid.GetFlowId()]++;
//		  if (Recv[myflowid.GetFlowId()]!=0)
//			  Avg[myflowid.GetFlowId()] = Delays[myflowid.GetFlowId()]/ float(Recv[myflowid.GetFlowId()]);
////
		  if (current_Delay <this->GetNode()->latency[myflowid.GetFlowId()])
		  {
			  this->GetNode()->reliable_received [myflowid.GetFlowId()]++;
		  }

		//  Delay[myflowid.GetFlowId()] += current_Delay;
		//  Recv[myflowid.GetFlowId()]++;
          if (InetSocketAddress::IsMatchingType (from))
            {
              NS_LOG_INFO ("TraceDelay: RX " << packet->GetSize () <<
                           " bytes from "<< InetSocketAddress::ConvertFrom (from).GetIpv4 () <<
                           " Sequence Number: " << currentSequenceNumber <<
                           " Uid: " << packet->GetUid () <<
                           " TXtime: " << seqTs.GetTs () <<
                           " RXtime: " << Simulator::Now () <<
                           " Delay: " << Simulator::Now () - seqTs.GetTs () <<
                           " Flowid: " << myflowid.GetFlowId()  << "Total Received:" << this->GetNode()->Recv[myflowid.GetFlowId()] <<
                           " reliable_received: " << this->GetNode()->reliable_received [myflowid.GetFlowId()] <<
                           " Average delay (ms):  " << this->GetNode()->Delays[myflowid.GetFlowId()].ToDouble(Time::MS) << "/" << this->GetNode()->Recv[myflowid.GetFlowId()] <<
                           "=" << this->GetNode()->Delays[myflowid.GetFlowId()].ToDouble(Time::MS)/float(this->GetNode()->Recv[myflowid.GetFlowId()]));
            }
          else if (Inet6SocketAddress::IsMatchingType (from))
            {
              NS_LOG_INFO ("TraceDelay: RX " << packet->GetSize () <<
                           " bytes from "<< Inet6SocketAddress::ConvertFrom (from).GetIpv6 () <<
                           " Sequence Number: " << currentSequenceNumber <<
                           " Uid: " << packet->GetUid () <<
                           " TXtime: " << seqTs.GetTs () <<
                           " RXtime: " << Simulator::Now () <<
                           " Delay: " << Simulator::Now () - seqTs.GetTs ()<<
                           " Flowid: "<< myflowid.GetFlowId()  <<
						   "Total Received:" << this->GetNode()->Recv[myflowid.GetFlowId()] <<
                           " reliable_received[Flowid]: " << this->GetNode()->reliable_received [myflowid.GetFlowId()] <<
                           " Average delay[Flowid]: " << this->GetNode()->Delays[myflowid.GetFlowId()] << "/" << this->GetNode()->Recv[myflowid.GetFlowId()] <<
                           "=" << this->GetNode()->Delays[myflowid.GetFlowId()]/float(this->GetNode()->Recv[myflowid.GetFlowId()]));
            }

          m_lossCounter.NotifyReceived (currentSequenceNumber);
          m_received++;
        }
    }
}

} // Namespace ns3
