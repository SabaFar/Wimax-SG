/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "packet-sink.h"
#include "ns3/flow-id-tag.h"
#include "ns3/delay-jitter-estimation.h"

//#include "upd-server.h" //no need

//#include "ns3/wimax-urban.h"
//#include "../src/wimax/examples/wimax-urban.h"
//#define FLOW_NUM 10
//int reliable_received[FLOW_NUM];
//ns3::Time latency[FLOW_NUM];
//ns3::Time Delays[FLOW_NUM];
//int Recv[FLOW_NUM];

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PacketSink");
NS_OBJECT_ENSURE_REGISTERED (PacketSink);

TypeId 
PacketSink::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PacketSink")
    .SetParent<Application> ()
    .AddConstructor<PacketSink> ()
    .AddAttribute ("Local", "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&PacketSink::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol", "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&PacketSink::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&PacketSink::m_rxTrace))
  ;
  return tid;
}

PacketSink::PacketSink ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
  m_prevUid = 0;
}

PacketSink::~PacketSink()
{
  //NS_LOG_FUNCTION (this);
}

uint32_t PacketSink::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
PacketSink::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
PacketSink::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void PacketSink::DoDispose (void)
{
  //NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void PacketSink::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      m_socket->Bind (m_local);
      m_socket->Listen ();
      m_socket->ShutdownSend ();
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&PacketSink::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&PacketSink::HandlePeerClose, this),
    MakeCallback (&PacketSink::HandlePeerError, this));
}

void PacketSink::StopApplication ()     // Called at time specified by Stop
{
  //NS_LOG_FUNCTION (this);
  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void PacketSink::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  FlowIdTag myflowid;
   //SeqTsHeader seqTs;
 // DelayJitterEstimationTime myTimeTag;
  //uint32_t currentSequenceNumber = seqTs.GetSeq ();
  //int64_t current_Delay = 0;
  DelayJitterEstimation myrxtime;


  Time current_Delay = Time(0);
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      myrxtime.RecordRx(packet);
      packet->FindFirstMatchingByteTag (myflowid);  //see http://www.nsnam.org/doxygen-release/wifi-phy-test_8cc_source.html
      //packet->FindFirstMatchingByteTag(myTimeTag);
      // uint32_t currentSequenceNumber = seqTs.GetSeq ();
    //  current_Delay = Simulator::Now()  - myTimeTag.GetTxTime();
      current_Delay = myrxtime.GetLastDelay();
      this->GetNode()->Delays[myflowid.GetFlowId()] += current_Delay;
      //if (packet->GetUid() != this->m_prevUid)

      this->GetNode()->Recv[myflowid.GetFlowId()]++;

      this->m_prevUid = packet->GetUid();

//      if (Recv[myflowid.GetFlowId()]!=0)
//      			  Avg[myflowid.GetFlowId()] = Delays[myflowid.GetFlowId()]/ float(Recv[myflowid.GetFlowId()]);
      if (current_Delay < this->GetNode()->latency[myflowid.GetFlowId()])
       {
    	  this->GetNode()->reliable_received [myflowid.GetFlowId()]++;
       }
      m_totalRx += packet->GetSize ();
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                       << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes"<< //" TXtime: " << myTimeTag.GetTxTime() <<
                       " RXtime: " << Simulator::Now () <<
                       " Delay: " << current_Delay <<
                       " Flowid: " <<  myflowid.GetFlowId() <<
                       " Tot Pkts Recv: " << this->GetNode()->Recv[myflowid.GetFlowId()] <<
                       " reliable_received: " << this->GetNode()->reliable_received [myflowid.GetFlowId()] <<
                       " Average delay (ms): " << this->GetNode()->Delays[myflowid.GetFlowId()].ToDouble(Time::MS) <<
                       "/" << this->GetNode()->Recv[myflowid.GetFlowId()] << "=" <<
                       this->GetNode()->Delays[myflowid.GetFlowId()].ToDouble(Time::MS)/float(this->GetNode()->Recv[myflowid.GetFlowId()]));

        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s packet sink received "
                       <<  packet->GetSize () << " bytes from "
                       << Inet6SocketAddress::ConvertFrom(from).GetIpv6 ()
                       << " port " << Inet6SocketAddress::ConvertFrom (from).GetPort ()
                       << " total Rx " << m_totalRx << " bytes"<<
                       " RXtime: " << Simulator::Now () <<
                       " Delay: " << current_Delay<< " Flowid: "
                       <<  myflowid.GetFlowId());
        }
      m_rxTrace (packet, from);
    }
}


void PacketSink::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 
void PacketSink::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 

void PacketSink::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
  m_socketList.push_back (s);
}

} // Namespace ns3