/*
 * @Author: 18746061711@163.com 18746061711@163.com
 * @Date: 2024-10-22 11:04:52
 * @LastEditors: 18746061711@163.com 18746061711@163.com
 * @LastEditTime: 2024-10-25 16:39:26
 * @FilePath: /minnow/src/tcp_sender.cc
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

/*
 * last_ack_seqno_.unwrap( isn_, bytes_send_ ) - 1 代表已收到的最后一个字节的序号
 */
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t res {}, bytes_acked {};
  isn_acked_ ? ( bytes_acked = last_ack_seqno_.unwrap( isn_, bytes_send_ ) ) : ( bytes_acked = 0 );
  res = bytes_send_ - bytes_acked;
  return res;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

/*
 * 比较分送segment最大 + 当前input buffer中剩余的字节数 + 接收窗口大小，取最小值作为本次发送的字节数
 * 记录已发送字节
 */
void TCPSender::push( const TransmitFunction& transmit )
{
  TCPSenderMessage msg = make_empty_message();
  if ( !isn_acked_ ) {
    msg.SYN = true;
    isn_sended_ = true;
  }

  std::vector<uint64_t> v_ { TCPConfig::MAX_PAYLOAD_SIZE, input_.reader().bytes_buffered(), recv_window_size_ };
  std::sort( v_.begin(), v_.end() );
  uint64_t bytes_can_send = v_[0];

  // 如果无数据可发且还没有收到isn的ack，则不发送
  if ( bytes_can_send == 0 && isn_acked_ )
    return;
  if ( ack_wrong_ )
    return;

  if ( bytes_can_send > 0 )
    msg.payload = std::string { input_.reader().peek(), 0, bytes_can_send };

  // 记录已发送字节
  bytes_send_ += msg.sequence_length();
  last_send_seqno_ = msg.seqno;
  transmit( msg );
}

/*
 * 主要对空消息的seqno进行处理
 * 构造一个空的segment，如果还没有收到ack，则seqno为isn
 * 因为消息empty，所以不对syn等标志赋值
 */
TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg {};
  if ( !isn_sended_ ) {
    msg.seqno = isn_;
  } else if ( !isn_acked_ ) {
    msg.seqno = isn_ + 1;
  } else if ( last_send_seqno_ + 1 == last_ack_seqno_ ) {
    msg.seqno = last_ack_seqno_;
  } else {
    Wrap32 seqno { Wrap32::wrap( bytes_send_, isn_ ) };
    msg.seqno = seqno;
  }
  return msg;
}

/*
 * 更新窗口大小，从bytes stream pop数据，更新last_ack_seqno_
 * 如果 last ack seqno 比之前收到的小，则忽视该消息
 */
void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( !msg.ackno.has_value() ) {
    ack_wrong_ = true;
    return;
  }
  if ( last_ack_seqno_.unwrap( isn_, bytes_send_ ) >= msg.ackno.value().unwrap( isn_, bytes_send_ ) ) {
    ack_wrong_ = true;
    return;
  }

  ack_wrong_ = false;
  input_.reader().pop( msg.ackno.value().unwrap( isn_, bytes_send_ )
                       - last_ack_seqno_.unwrap( isn_, bytes_send_ ) );
  recv_window_size_ = msg.window_size;
  last_ack_seqno_ = msg.ackno.value();

  if ( last_ack_seqno_ != isn_ )
    isn_acked_ = true;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;
}
