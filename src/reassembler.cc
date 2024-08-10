#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <tuple>

using namespace std;

void Reassembler::insert( uint64_t data_first_idx, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    recv_eof = true;
    uint64_t tmp_eof_idx { 0 };
    if ( data_first_idx + data.size() >= 1 )
      tmp_eof_idx = data_first_idx + data.size() - 1;
    eof_idx = eof_idx >= tmp_eof_idx ? eof_idx : tmp_eof_idx;
  }

  if ( recv_eof && ( eof_idx == 0 || reassemble_header_idx == eof_idx + 1 ) ) {
    if ( data.empty() )
      output_.writer().close();
  }

  if ( data.empty() ) {
    return;
  }

  uint64_t writer_capacity = writer().available_capacity();

  /* 遍历 unassembled_head_tail_idxs_, 判断是否对其进行变更 */
  uint64_t reassemble_end_idx = reassemble_header_idx + writer_capacity - 1;
  uint64_t data_last_idx = data_first_idx + data.size() - 1;
  uint64_t begin_idx = data_first_idx >= reassemble_header_idx ? data_first_idx : reassemble_header_idx;
  uint64_t last_idx = reassemble_end_idx <= data_last_idx ? reassemble_end_idx : data_last_idx;

  if ( data_first_idx > reassemble_end_idx || data_last_idx < reassemble_header_idx )
    return;

  auto head_node = data_unassembled_.begin();
  while ( 1 ) {
    if ( data_unassembled_.empty() ) {
      data_unassembled_.push_back(
        { data.substr( begin_idx - data_first_idx, last_idx - begin_idx + 1 ), begin_idx, last_idx } );
      break;
    }

    if ( begin_idx <= std::get<1>( *head_node ) ) {
      // 情况(1)
      if ( last_idx < std::get<1>( *head_node ) ) {
        auto substr = data.substr( begin_idx - data_first_idx, last_idx - begin_idx + 1 );
        data_unassembled_.insert( head_node, { substr, begin_idx, last_idx } );
        break;
      } else {
        auto substr = data.substr( begin_idx - data_first_idx, std::get<1>( *head_node ) - begin_idx );
        *head_node = { substr + std::get<0>( *head_node ), begin_idx, std::get<2>( *head_node ) };
        // 情况(2)
        if ( last_idx <= std::get<2>( *head_node ) ) {
          break;
        }
        // 情况(3)
        else {
          begin_idx = std::get<2>( *head_node ) + 1;
          head_node++;
          continue;
        }
      }
    } else {
      if ( begin_idx <= std::get<2>( *head_node ) ) {
        // 情况(4)
        if ( last_idx <= std::get<2>( *head_node ) ) {
          break;
        }
        // 情况(5)
        else {
          begin_idx = std::get<2>( *head_node ) + 1;
          head_node++;
          continue;
        }
      }
      // 情况(6)
      else {
        head_node++;
        continue;
      }
    }

    if ( head_node == data_unassembled_.end() )
      break;
  }

  // 遍历 unassembled_head_tail_idxs_, 判断是否对其进行写入
  head_node = data_unassembled_.begin();
  while ( head_node != data_unassembled_.end() && std::get<1>( *head_node ) == reassemble_header_idx ) {
    output_.writer().push( std::get<0>( *head_node ) );
    reassemble_header_idx = std::get<2>( *head_node ) + 1;
    head_node++;
    data_unassembled_.pop_front();

    if ( recv_eof && reassemble_header_idx == eof_idx + 1 ) {
      output_.writer().close();
    }
  }
}

/* 遍历 unassembled_head_tail_idxs_ 实现 */
uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return std::accumulate( data_unassembled_.begin(),
                          data_unassembled_.end(),
                          0,
                          []( int runningTotal, const std::tuple<std::string, int, int>& p ) {
                            return runningTotal + ( std::get<2>( p ) - std::get<1>( p ) + 1 );
                          } );
  ;
}
