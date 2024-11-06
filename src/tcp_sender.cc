/*
 * @Author: 18746061711@163.com 18746061711@163.com
 * @Date: 2024-10-22 11:04:52
 * @LastEditors: 18746061711@163.com 18746061711@163.com
 * @LastEditTime: 2024-10-31 14:33:34
 * @FilePath: /minnow/src/tcp_sender.cc
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include <cassert>
#include <cstdint>

using namespace std;

/*
 * last_ack_seqno_.unwrap( isn_, bytes_send_ ) - 1 代表已收到的最后一个字节的序号
 */
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t res { 0 };
  for ( auto& msg : messages_in_flight_ ) {
    res += msg.sequence_length();
  }
  return res;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return consecutive_retrans_num_;
}

/*
 * 比较分送segment最大 + 当前input buffer中剩余的字节数 + 接收窗口大小，取最小值作为本次发送的字节数
 * 记录已发送字节
 */
void TCPSender::push( const TransmitFunction& transmit )
{
  // 新消息逻辑
  TCPSenderMessage msg = make_empty_message();
  if ( msg.RST ) {
    transmit( msg );
    return;
  }

  if ( msg.seqno == isn_ )
    msg.SYN = true;
  if ( input_.writer().is_closed() && !Fin_sent_ )
    msg.FIN = true;

  std::vector<uint64_t> v_ { TCPConfig::MAX_PAYLOAD_SIZE, input_.reader().bytes_buffered(), recv_window_size_ };
  std::sort( v_.begin(), v_.end() );

  uint64_t payload_size = v_[0];
  if ( msg.SYN && payload_size > 0 && payload_size == recv_window_size_ ) {
    recv_window_size_ -= 1;
    payload_size -= 1;
  } else if ( msg.SYN && payload_size > 0 && payload_size < recv_window_size_ ) {
    recv_window_size_ -= 1;
  }

  if ( recv_win_zero_ && recv_window_size_ == 0 && input_.reader().bytes_buffered() != 0 ) {
    payload_size = 1;
    recv_win_zero_ = false;
    send_win_zero_ = true;
  }

  if ( payload_size == 0 && !msg.SYN && !msg.FIN )
    return;

  // 处理payload
  if ( payload_size > 0 ) {
    msg.payload = std::string { input_.reader().peek(), 0, payload_size };
    input_.reader().pop( payload_size );
    if ( recv_window_size_ >= payload_size )
      recv_window_size_ -= payload_size;
  }

  // 处理标志位
  if ( recv_window_size_ == 0 && !recv_win_zero_ ) {
    msg.FIN = false;
  }
  if ( msg.FIN && msg.sequence_length() > TCPConfig::MAX_PAYLOAD_SIZE && input_.reader().bytes_buffered() != 0 ) {
    msg.FIN = false;
  }
  if ( recv_win_zero_ ) {
    recv_win_zero_ = false;
    send_win_zero_ = true;
  }
  if ( msg.FIN )
    Fin_sent_ = true;

  if ( msg.sequence_length() == 0 )
    return;

  // 记录已发送字节
  messages_in_flight_.push_back( msg );
  seqs_sent_len_ += msg.sequence_length();
  transmit( msg );

  if ( recv_window_size_ > 0 && input_.reader().bytes_buffered() > 0 )
    push( transmit );
}

/*
 * 如果还有outstanding segment，seqno 选择未发送的最小seqno
 */
TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg {};

  if ( !messages_in_flight_.empty() ) {
    msg.seqno = messages_in_flight_.back().seqno + messages_in_flight_.back().sequence_length();
  } else {
    msg.seqno = last_ack_seqno_;
  }

  if ( input_.has_error() )
    msg.RST = true;

  return msg;
}

/*
 * 更新窗口大小，从bytes stream pop数据，更新last_ack_seqno_
 * 如果 last ack seqno 比之前收到的小，则忽视该消息
 */
void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 计算最旧 outstanding segment 最后一个字节 seqno
  auto calc_seqno = [this]( TCPSenderMessage calc_msg ) {
    return calc_msg.seqno.unwrap( isn_, seqs_sent_len_ ) + calc_msg.sequence_length();
  };

  if ( msg.RST ) {
    input_.set_error();
  }

  if ( !msg.ackno.has_value() ) {
    recv_window_size_ = msg.window_size;
    return;
  }

  uint64_t seqno_msg = msg.ackno.value().unwrap( isn_, seqs_sent_len_ );
  // 如果返回ack < outstanding头部，则将其当作错误消息
  if ( !messages_in_flight_.empty()
       && seqno_msg < messages_in_flight_.front().seqno.unwrap( isn_, seqs_sent_len_ ) )
    return;
  // 如果返回ack比所有发送ack都大，则将其当作错误消息
  if ( !messages_in_flight_.empty() && seqno_msg > calc_seqno( messages_in_flight_.back() ) )
    return;

  bool outstanding_recv { false };
  while ( !messages_in_flight_.empty() ) {
    uint64_t seqno_calc = calc_seqno( messages_in_flight_.front() );
    if ( seqno_msg >= seqno_calc ) {
      messages_in_flight_.pop_front();
      outstanding_recv = true;
      send_win_zero_ = false;
    } else {
      break;
    }
  }

  // 更新 window size 和 last ack seqno 逻辑
  if ( !messages_in_flight_.empty() ) {
    uint64_t seq_length_outstanding { 0 };
    for ( auto& out_msg : messages_in_flight_ )
      seq_length_outstanding += out_msg.sequence_length();

    if ( msg.window_size >= seq_length_outstanding )
      recv_window_size_ = msg.window_size - seq_length_outstanding;
    else
      return;
  } else {
    if ( last_ack_seqno_.unwrap( isn_, seqs_sent_len_ ) > msg.ackno.value().unwrap( isn_, seqs_sent_len_ ) )
      return;
    last_ack_seqno_ = msg.ackno.value();
    recv_window_size_ = msg.window_size;
  }

  if ( outstanding_recv ) {
    retransmit_flag_ = false;
    consecutive_retrans_num_ = 0;
    curr_RTO_ms_ = initial_RTO_ms_;
    countdown_ms_ = initial_RTO_ms_;
  }

  recv_win_zero_ = ( msg.window_size == 0 ? true : false );
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( messages_in_flight_.empty() ) {
    return;
  }

  if ( countdown_ms_ > ms_since_last_tick ) {
    countdown_ms_ -= ms_since_last_tick;
    return;
  }

  transmit( messages_in_flight_.front() );

  if ( recv_window_size_ == 0 && messages_in_flight_.front().sequence_length() != 0 && send_win_zero_ ) {
    curr_RTO_ms_ = initial_RTO_ms_;
  } else {
    curr_RTO_ms_ *= 2;
  }

  countdown_ms_ = curr_RTO_ms_;
  retransmit_flag_ = true;
  consecutive_retrans_num_++;
}
