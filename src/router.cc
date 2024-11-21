/*
 * @Author: DFZ 18746061711@163.com
 * @Date: 2024-11-16 14:08:06
 * @LastEditors: DFZ 18746061711@163.com
 * @LastEditTime: 2024-11-21 09:40:51
 * @FilePath: /minnow/src/router.cc
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  _route_table[interface_num].push_back( { route_prefix, prefix_length, next_hop } );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  for ( auto interface : _interfaces ) {

    while ( !interface->datagrams_received().empty() ) {

      auto& datagram = interface->datagrams_received().front();
      route_( datagram );
      interface->datagrams_received().pop();
    }
  }
}

// 处理网络接口消息的路由
void Router::route_( InternetDatagram datagram )
{
  if ( datagram.header.ttl <= 1 ) {
    return;
  }

  bool match = false;
  uint8_t match_size = 0;
  std::shared_ptr<NetworkInterface> next_hop { nullptr };
  std::unique_ptr<Address> ip { nullptr };

  for ( size_t idx = 0; idx < _interfaces.size(); idx++ ) {

    for ( auto item : _route_table[idx] ) {

      if ( !( match_( datagram.header.dst, item ) && item.prefix_length >= match_size ) ) {
        continue;
      }

      match = true;
      match_size = item.prefix_length;
      next_hop = interface( idx );
      item.next_hop.has_value()
        ? ip = std::make_unique<Address>( item.next_hop.value() )
        : ip = std::make_unique<Address>( Address::from_ipv4_numeric( datagram.header.dst ) );
    }
  }

  if ( match ) {
    datagram.header.ttl--;
    next_hop->send_datagram( datagram, *ip );
  }
}

bool Router::match_( uint32_t ip, Router::RouteItem item )
{
  bool res = true;

  for ( int i = 32 - item.prefix_length; i < 32 - 1; i++ ) {
    if ( ( ( ip >> i ) & 1 ) != ( ( item.route_prefix >> i ) & 1 ) ) {
      res = false;
      break;
    }
  }

  return res;
}
