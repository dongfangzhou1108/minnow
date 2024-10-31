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
  if ( msg.seqno == isn_ )
    msg.SYN = true;
  if ( input_.writer().is_closed() )
    msg.FIN = true;

  std::vector<uint64_t> v_ { TCPConfig::MAX_PAYLOAD_SIZE, input_.reader().bytes_buffered(), recv_window_size_ };
  std::sort( v_.begin(), v_.end() );
  uint64_t payload_size = v_[0];

  if ( Fin_ack_ )
    return;

  if ( msg.FIN && Close_ack_ ) {
    // FIN not acked test
    if ( Fin_sent_ )
      return;

    msg.payload = std::string { input_.reader().peek(), 0, payload_size };

    // SYN + FIN & Don't add FIN if this would make the segment exceed the receiver's window
    if ( msg.sequence_length() > payload_size && payload_size == recv_window_size_ && payload_size != 0 )
      msg.FIN = false;
    if ( sequence_numbers_in_flight() + msg.sequence_length() > recv_window_size_ )
      return;
  } else if ( msg.FIN && !Close_ack_ && !messages_in_flight_.empty() && recv_window_size_ == 0 ) {
    return;
  } else if ( msg.FIN && !Close_ack_ && messages_in_flight_.empty() ) {
    msg.seqno = last_ack_seqno_;
    msg.payload = std::string { input_.reader().peek(), 0, payload_size }; // FIN with data
    messages_in_flight_.push_back( msg );
    Fin_sent_ = true;
    transmit( msg );
    return;
  }

  if ( payload_size + msg.sequence_length() > 0 ) {
    msg.payload = std::string { input_.reader().peek(), 0, payload_size };
    input_.reader().pop( payload_size );
    recv_window_size_ -= payload_size;
  } else
    return;

  // 记录已发送字节
  messages_in_flight_.push_back( msg );
  seqs_sent_len_ += msg.sequence_length();
  if ( msg.FIN )
    Fin_sent_ = true;
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

  return msg;
}

/*
 * 更新窗口大小，从bytes stream pop数据，更新last_ack_seqno_
 * 如果 last ack seqno 比之前收到的小，则忽视该消息
 */
void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 计算最旧 outstanding segment 最后一个字节 seqno
  auto calc_seqno = [this]() {
    return messages_in_flight_.front().seqno.unwrap( isn_, seqs_sent_len_ )
           + messages_in_flight_.front().sequence_length();
  };

  // 计算最新 outstanding segment 最后一个字节 seqno
  auto calc_seqno_max = [this]() {
    return messages_in_flight_.back().seqno.unwrap( isn_, seqs_sent_len_ )
           + messages_in_flight_.back().sequence_length();
  };

  if ( !msg.ackno.has_value() )
    return;
  if ( messages_in_flight_.empty() )
    return;
  if ( msg.ackno.value().unwrap( isn_, seqs_sent_len_ ) < calc_seqno() && !( input_.writer().is_closed() ) )
    return;
  // credit for test: Jared Wasserman (2020)
  if ( msg.ackno.value().unwrap( isn_, seqs_sent_len_ ) > calc_seqno_max() )
    return;

  last_ack_seqno_ = msg.ackno.value();
  recv_window_size_ = msg.window_size;

  retransmit_flag_ = false;
  consecutive_retrans_num_ = 0;
  curr_RTO_ms_ = initial_RTO_ms_;
  countdown_ms_ = initial_RTO_ms_;

  // 处理 FIN 确认
  if ( input_.writer().is_closed() ) {
    Close_ack_ = true;
  }

  while ( !messages_in_flight_.empty() ) {
    if ( msg.ackno.value().unwrap( isn_, seqs_sent_len_ ) >= calc_seqno() ) {
      messages_in_flight_.pop_front();
    } else {
      break;
    }
  }

  if ( Close_ack_ && messages_in_flight_.empty() && Fin_sent_ )
    Fin_ack_ = true;
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

  curr_RTO_ms_ *= 2;
  countdown_ms_ = curr_RTO_ms_;
  retransmit_flag_ = true;
  consecutive_retrans_num_++;
  transmit( messages_in_flight_.front() );
}
