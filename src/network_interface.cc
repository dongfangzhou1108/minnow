/*
 * @Author: DFZ 18746061711@163.com
 * @Date: 2024-11-16 14:08:06
 * @LastEditors: DFZ 18746061711@163.com
 * @LastEditTime: 2024-11-19 12:00:11
 * @FilePath: /minnow/src/network_interface.cc
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  /*
   * 如果能够识别next_hop的MAC地址
   * 	则直接发送数据包
   * 否则
   * 	发送ARP请求（相同IP在5S内不允许重复发送）
   *  维护待发送消息&IP
   */

  EthernetFrame ip_frame;
  ip_frame.header.src = ethernet_address_;
  ip_frame.header.type = EthernetHeader::TYPE_IPv4;
  Serializer s;
  dgram.serialize( s );
  ip_frame.payload = s.output();

  if ( IPV4_ARPed_.contains( next_hop.ipv4_numeric() ) ) {
    ip_frame.header.dst = IPV4_ARPed_[next_hop.ipv4_numeric()].first;
    transmit( ip_frame );
  } else {
    // 维护待发送消息
    packets_to_send_[next_hop.ipv4_numeric()].push( ip_frame );
    // 维护ARP请求
    if ( IPV4_ARPing_.contains( next_hop.ipv4_numeric() ) && IPV4_ARPing_[next_hop.ipv4_numeric()] < 5000 ) {
      return;
    }
    IPV4_ARPing_[next_hop.ipv4_numeric()] = 0;
    // 发送ARP请求
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ethernet_address = ethernet_address_;
    arp.sender_ip_address = ip_address_.ipv4_numeric();
    arp.target_ip_address = next_hop.ipv4_numeric();
    Serializer s_arp;
    arp.serialize( s_arp );
    EthernetFrame arp_frame;
    arp_frame.header.dst = ETHERNET_BROADCAST;
    arp_frame.header.src = ethernet_address_;
    arp_frame.header.type = EthernetHeader::TYPE_ARP;
    arp_frame.payload = s_arp.output();
    transmit( arp_frame );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  /*
   * 如果 IPV4 数据包
   * 	推送数据队列并维护
   * 否则，如果 ARP 请求
   * 	发送 ARP 响应（实验中默认得到正确回复）
   * 否则，如果 ARP 响应
   * 	维护 ARP 缓存
   */

  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  Parser p( frame.payload );
  InternetDatagram ip_frame;
  ARPMessage arp;
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    ip_frame.parse( p );
    datagrams_received().push( ip_frame );
    if ( ip_frame.header.dst != ip_address_.ipv4_numeric() ) {
      return;
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    arp.parse( p );
    if ( arp.target_ip_address != ip_address_.ipv4_numeric() ) {
      return;
    }
  }

  if ( frame.header.type == EthernetHeader::TYPE_ARP && arp.opcode == ARPMessage::OPCODE_REQUEST ) {
    // learn from ARP request
    if ( !IPV4_ARPed_.contains( arp.sender_ip_address ) ) {
      IPV4_ARPed_[arp.sender_ip_address] = { arp.sender_ethernet_address, 0 };
    }
    // ARP 请求
    ARPMessage arp_reply;
    arp_reply.opcode = ARPMessage::OPCODE_REPLY;
    arp_reply.sender_ethernet_address = ethernet_address_;
    arp_reply.target_ethernet_address = arp.sender_ethernet_address;
    arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
    arp_reply.target_ip_address = arp.sender_ip_address;
    Serializer s_arp;
    arp_reply.serialize( s_arp );
    EthernetFrame arp_frame;
    arp_frame.header.dst = frame.header.src;
    arp_frame.header.src = ethernet_address_;
    arp_frame.header.type = EthernetHeader::TYPE_ARP;
    arp_frame.payload = s_arp.output();
    transmit( arp_frame );
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP && arp.opcode == ARPMessage::OPCODE_REPLY ) {
    // ARP 响应
    IPV4_ARPing_.erase( IPV4_ARPing_.find( arp.sender_ip_address ) );
    IPV4_ARPed_[arp.sender_ip_address] = { arp.sender_ethernet_address, 0 };
    while ( !packets_to_send_[arp.sender_ip_address].empty() ) {
      packets_to_send_[arp.sender_ip_address].front().header.dst = arp.sender_ethernet_address;
      transmit( packets_to_send_[arp.sender_ip_address].front() );
      packets_to_send_[arp.sender_ip_address].pop();
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  /*
   * 维护 ARP 缓存（最长维护时间30S）
   */
  std::for_each( IPV4_ARPing_.begin(), IPV4_ARPing_.end(), [ms_since_last_tick]( auto& iter ) {
    iter.second += ms_since_last_tick;
  } );

  std::for_each( IPV4_ARPed_.begin(), IPV4_ARPed_.end(), [ms_since_last_tick]( auto& iter ) {
    iter.second.second += ms_since_last_tick;
  } );

  std::erase_if( IPV4_ARPed_, []( const auto& item ) {
    auto const& [key, value] = item;
    return value.second > 30000;
  } );
}
