/*
 * @Author: DFZ 18746061711@163.com
 * @Date: 2024-11-16 14:08:06
 * @LastEditors: DFZ 18746061711@163.com
 * @LastEditTime: 2024-11-16 14:22:51
 * @FilePath: /minnow/apps/webget.cc
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置:
 * https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
// #include <sys/socket.h>
#include "../util/tcp_minnow_socket.hh"

using namespace std;

void get_URL( const string& host, const string& path )
{
  Address addr { host, "http" };
  TCPSocket sock {};

  sock.connect( addr );
  std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
  sock.write( req );

  std::string result {};
  while ( !sock.eof() ) {
    std::string recv {};
    sock.read( recv );
    result += recv;
  }
  std::cout << result;

  sock.close();
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
