#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 {
    static_cast<uint32_t>( ( n + zero_point.raw_value_ ) % ( static_cast<uint64_t>( UINT32_MAX ) + 1 ) ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t res_mod { ( raw_value_ >= zero_point.raw_value_ )
                       ? ( raw_value_ - zero_point.raw_value_ )
                       : ( raw_value_ + UINT32_MAX + 1 - zero_point.raw_value_ ) };
  /*
   * res_bigger >= checkpoint
   * res_smaller < checkpoint
   * res = min(res_bigger - checkpoint, checkpoint - res_smaller)
   * res % 2^32 = res_mode
   */
  uint64_t tmp
    = checkpoint / ( static_cast<uint64_t>( UINT32_MAX ) + 1 ) * ( static_cast<uint64_t>( UINT32_MAX ) + 1 );
  uint64_t res_bigger
    = tmp + res_mod >= checkpoint ? tmp + res_mod : tmp + res_mod + static_cast<uint64_t>( UINT32_MAX ) + 1;
  uint64_t res_smaller { UINT64_MAX };
  if ( tmp + res_mod < checkpoint )
    res_smaller = tmp + res_mod;
  else if ( tmp + res_mod >= static_cast<uint64_t>( UINT32_MAX ) + 1 )
    res_smaller = tmp + res_mod - static_cast<uint64_t>( UINT32_MAX ) - 1;
  else
    res_smaller = res_bigger;
  return res_bigger - checkpoint <= checkpoint - res_smaller ? res_bigger : res_smaller;
}
