/*
 * @Author: DFZ 18746061711@163.com
 * @Date: 2024-11-16 14:08:06
 * @LastEditors: DFZ 18746061711@163.com
 * @LastEditTime: 2024-11-20 13:49:57
 * @FilePath: /minnow/src/router.hh
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <memory>
#include <optional>

#include "exception.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();
  void route_( InternetDatagram );
  struct RouteItem
  {
    uint32_t route_prefix;
    uint8_t prefix_length;
    std::optional<Address> next_hop;
  };
  bool match_( uint32_t, RouteItem );

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};

  /*
   * key size_t: _interfaces 序号
   */
  std::map<size_t, std::vector<RouteItem>> _route_table {};
};
