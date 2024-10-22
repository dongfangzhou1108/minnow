/*
 * @Author: 18746061711@163.com 18746061711@163.com
 * @Date: 2024-10-21 18:39:29
 * @LastEditors: 18746061711@163.com 18746061711@163.com
 * @LastEditTime: 2024-10-22 10:53:31
 * @FilePath: /minnow/src/tcp_sender.hh
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , countdown_RTO_ms_( initial_RTO_ms )
    , last_ack_seqno_( isn_ )
    , last_send_seqno_( isn_ )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  uint64_t countdown_RTO_ms_;                   // rto翻倍倒计时
  std::map<uint64_t, uint64_t> seqno_timer_ {}; // 超时计时器，key为seq，value为已发送时间，tick后更新
  bool isn_acked_ { false }, isn_sended_ { false }; // isn 是否被确认
  Wrap32 last_ack_seqno_, last_send_seqno_;         // 最后收到的ack的seq号, 最后发送的seq号
  uint64_t recv_window_size_ {}, bytes_send_ {};    // 接收窗口大小

  uint64_t consecutive_retrans_num_ {}; // 连续重传次数
};
