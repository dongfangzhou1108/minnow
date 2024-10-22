#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t res {}, bytes_acked {};
  isn_acked_ ? ( bytes_acked = last_ack_seqno_.unwrap( isn_, bytes_send_ ) - 1 ) : ( bytes_acked = 0 );
  res = bytes_send_ - bytes_acked;
  return res;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

/*
 * 比较分送segment最大值和当前input buffer中剩余的字节数，取最小值作为本次发送的字节数
 * 记录已发送字节
 */
void TCPSender::push( const TransmitFunction& transmit )
{
  TCPSenderMessage msg = make_empty_message();
  if ( !isn_acked_ ) {
    msg.SYN = true;
    isn_sended_ = true;
  }

  uint64_t bytes_can_send = std::min( TCPConfig::MAX_PAYLOAD_SIZE, input_.reader().bytes_buffered() );
  if ( bytes_can_send > 0 )
    msg.payload = std::string { input_.reader().peek(), 0, bytes_can_send };

  // 记录已发送字节
  bytes_send_ += msg.sequence_length();
  transmit( msg );
}

/*
 * 构造一个空的segment，如果还没有收到ack，则seqno为isn，否则为last_ack_seqno_
 * 因为消息empty，所以不对syn等标志赋值
 */
TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg {};
  if ( !isn_sended_ ) {
    msg.seqno = isn_;
  } else if ( !isn_acked_ ) {
    msg.seqno = isn_ + 1;
  } else {
    msg.seqno = last_ack_seqno_;
  }
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;
}
