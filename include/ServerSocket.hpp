#ifndef SERVER_SOCKET_HPP
#define SERVER_SOCKET_HPP

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>

#include "Client.hpp"
#include "ServerBlock.hpp"
#include "SocketAddress.hpp"
#include "constant.hpp"

class ServerSocket {
 public:
  ServerSocket();
  ServerSocket(const ServerSocket& src);
  ~ServerSocket();

  ServerSocket& operator=(const ServerSocket& src);

  void open();
  void bind(const SocketAddress& address, int backlog);
  Client accept(const ServerBlock& default_server) const;
  int getFD() const;
  const SocketAddress& getAddress() const;

 private:
  int fd_;
  SocketAddress address_;

 public:
  class SocketOpenException : public std::exception {
   public:
    SocketOpenException(const char* cause);
    const char* what() const throw();

   private:
    const char* cause;
  };

  class SocketBindException : public std::exception {
   public:
    SocketBindException(const char* cause);
    const char* what() const throw();

   private:
    const char* cause;
  };

  class SocketAcceptException : public std::exception {
   public:
    SocketAcceptException(const char* cause);
    const char* what() const throw();

   private:
    const char* cause;
  };
};

#endif