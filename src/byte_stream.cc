#include "byte_stream.hh"
#include <cstdint>
#include <sys/types.h>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  if ( is_closed() || available_capacity() == 0 )
    return;

  uint64_t bytes_write_now = data.size() <= available_capacity() ? data.size() : available_capacity();
  this->data_.append( data, 0, bytes_write_now );
  unread_bytes_ += bytes_write_now;

  written_bytes_ += bytes_write_now;

  return;
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - unread_bytes_;
}

uint64_t Writer::bytes_pushed() const
{
  return written_bytes_;
}

bool Reader::is_finished() const
{
  return 0 == unread_bytes_ && closed_;
}

uint64_t Reader::bytes_popped() const
{
  return read_bytes_;
}

string_view Reader::peek() const
{
  return string_view( data_.data(), data_.size() );
}

void Reader::pop( uint64_t len )
{
  uint64_t pop_bytes = len < unread_bytes_ ? len : unread_bytes_;
  data_.erase( 0, pop_bytes );
  unread_bytes_ -= pop_bytes;
  read_bytes_ += pop_bytes;
}

uint64_t Reader::bytes_buffered() const
{
  return unread_bytes_;
}
