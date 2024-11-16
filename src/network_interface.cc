/*
 * @Author: DFZ 18746061711@163.com
 * @Date: 2024-11-16 14:08:06
 * @LastEditors: DFZ 18746061711@163.com
 * @LastEditTime: 2024-11-16 16:07:09
 * @FilePath: /minnow/src/network_interface.cc
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
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

	EthernetFrame frame;
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
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
	/*
	 * 维护 ARP 缓存（最长维护时间30S）
	 */
}
