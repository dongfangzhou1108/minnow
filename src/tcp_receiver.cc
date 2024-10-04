#include "tcp_receiver.hh"
#include <iostream>
using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    rst_enable_ = true;
    reassembler_.reader().set_error();
  }

  if ( message.SYN ) {
    ackno_enable_ = true;
    isn_ = message.seqno;
    checkpoint_ = 0;
  }

  // for test case "byte with invalid stream index should be ignored"
  if ( ackno_enable_ && !message.SYN && message.seqno == isn_ && !message.payload.empty() ) {
    return;
  }

  uint64_t stream_index = message.seqno.unwrap( isn_, checkpoint_ );
  stream_index >= 1 ? stream_index-- : stream_index;
  reassembler_.insert( stream_index, message.payload, message.FIN );
  checkpoint_ = reassembler_.writer().bytes_pushed() + 1;

  if ( message.FIN ) {
    stream_length_ = stream_index + 1 + message.payload.size();
  }

  if ( !ackno_enable_ ) {
    return;
  }

  if ( checkpoint_ == stream_length_ ) {
    ackno_ = Wrap32::wrap( checkpoint_ + 1, isn_ );
  } else {
    ackno_ = Wrap32::wrap( checkpoint_, isn_ );
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage res;

  if ( rst_enable_ || reassembler_.reader().has_error() ) {
    res.RST = true;
  }

  if ( ackno_enable_ ) {
    res.ackno = ackno_;
  }

  res.window_size = static_cast<uint64_t>( UINT16_MAX ) <= reassembler_.writer().available_capacity()
                      ? UINT16_MAX
                      : reassembler_.writer().available_capacity();

  return res;
}
