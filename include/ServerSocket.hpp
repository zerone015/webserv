#ifndef SERVER_SOCKET_HPP
#define SERVER_SOCKET_HPP

#include <fcntl.h>
#include <sys/socket.h>

#include <set>
#include <sstream>

#include "Client.hpp"
#include "InetSocketAddress.hpp"
#include "constant.hpp"

class ServerSocket {
 public:
  ServerSocket();
  ServerSocket(const ServerSocket& src);
  virtual ~ServerSocket();

  ServerSocket& operator=(const ServerSocket& src);

  void open();
  void bind(const InetSocketAddress& address, int backlog);
  Client accept() const;
  int getSocket() const;
  const InetSocketAddress& getAddress() const;

 private:
  int _socket;
  InetSocketAddress _address;

 public:
  class SocketOpenException : public std::exception {
   public:
    SocketOpenException(const char* cause);

   private:
    const char* cause;

    const char* what() const throw();
  };

  class SocketBindException : public std::exception {
   public:
    SocketBindException(const char* cause);

   private:
    const char* cause;

    const char* what() const throw();
  };

  class SocketSetFlagException : public std::exception {
   public:
    SocketSetFlagException(const char* cause);

   private:
    const char* cause;

    const char* what() const throw();
  };

  class SocketAcceptException : public std::exception {
   public:
    SocketAcceptException(const char* cause);

   private:
    const char* cause;

    const char* what() const throw();
  };
};

#endif