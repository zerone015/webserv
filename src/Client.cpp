#include "Client.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>

#include "Log.hpp"
#include "constant.hpp"

Client::Client(int fd, const ServerBlock& default_server,
               const SocketAddress& cli_addr, const SocketAddress& serv_addr)
    : fd_(fd),
      cli_address_(cli_addr),
      serv_address_(serv_addr),
      response_obj_(default_server) {
  logConnectionInfo();
}

Client::Client(const Client& src)
    : fd_(src.fd_),
      cli_address_(src.cli_address_),
      serv_address_(src.serv_address_),
      parser_(src.parser_),
      response_obj_(src.response_obj_),
      cgi_(src.cgi_),
      buf_(src.buf_) {}

Client::~Client() {}

int Client::getFD() const { return fd_; }

HttpParser& Client::getParser() { return parser_; }

const HttpParser& Client::getParser() const { return parser_; }

std::string Client::getSocketKey() const {
  return serv_address_.getIP() + ":" + serv_address_.getPort();
}

const HttpRequest& Client::getRequestObj() const {
  return parser_.getRequestObj();
}

HttpResponse& Client::getResponseObj() { return response_obj_; }

const HttpResponse& Client::getResponseObj() const { return response_obj_; }

Cgi& Client::getCgi() { return cgi_; }

const Cgi& Client::getCgi() const { return cgi_; }

const SocketAddress& Client::getServerAddress() const { return serv_address_; }

const SocketAddress& Client::getClientAddress() const { return cli_address_; }

std::string Client::receive() const {
  char buf[BUF_SIZE];

  std::size_t read_bytes = recv(fd_, &buf, BUF_SIZE, 0);

  if (read_bytes == ERROR<std::size_t>()) {
    std::cerr << "[Error] Receive failed: " << strerror(errno) << '\n';
    throw ResponseException(C500);
  }
  if (read_bytes == 0) {
    throw ConnectionClosedException();
  }
  std::string request(buf, read_bytes);
  logReceiveInfo(request);
  return request;
}

void Client::send() {
  if (!isPartialWritten()) {
    buf_ = response_obj_.generate(parser_.getRequestObj(), isCgi(),
                                  cgi_.getResponse());
  }
  std::size_t write_bytes = ::send(fd_, buf_.c_str(), buf_.size(), 0);

  if (write_bytes == ERROR<std::size_t>()) {
    std::cerr << "[Error] Send failed: " << strerror(errno) << '\n';
    throw ConnectionClosedException();
  }

  Log::header("Send Information");
  logAddressInfo();
  std::cout << "[Send Data] " << '\n' << buf_.substr(0, write_bytes) << '\n';
  Log::footer("Send");

  buf_.erase(0, write_bytes);
}

void Client::executeCgiIO() {
  try {
    Selector cgi_selector;
    const int* pipe_fds = cgi_.getPipeFds();

    cgi_selector.registerFD(pipe_fds[WRITE]);
    cgi_selector.registerFD(pipe_fds[READ]);

    if (cgi_selector.select() > 0) {
      if (cgi_.hasBody() && cgi_selector.isWritable(pipe_fds[WRITE])) {
        cgi_.writeToPipe();
      }
      if (cgi_selector.isReadable(pipe_fds[READ])) {
        cgi_.readToPipe();
      }
    }
  } catch (const std::exception& e) {
    response_obj_.setStatus(C500);
    std::cerr << "[Error] Cgi IO failed: " << e.what() << '\n';
  }
}

void Client::close() {
  ::close(fd_);
  fd_ = -1;

  Log::header("Close Connection Information");
  logAddressInfo();
  Log::footer("Close Connection");
}

void Client::clearBuffer() { buf_.clear(); }

void Client::clear() {
  parser_.clear();
  cgi_.clear();
  response_obj_.clear();
}

bool Client::isCgi() const {
  const LocationBlock* location_block = response_obj_.getLocationBlock();
  if (!location_block) return false;
  const HttpRequest& request_obj = parser_.getRequestObj();
  return location_block->isCgi(request_obj.getUri(), request_obj.getMethod(),
                               request_obj.getQueryString());
}

bool Client::isPartialWritten() const { return !buf_.empty(); }

bool Client::isReadyToCgiIO() const {
  if (parser_.isCompleted() && isCgi() && !cgi_.isCompleted() &&
      response_obj_.isSuccessCode()) {
    return true;
  }
  return false;
}

bool Client::isReadyToSend() const {
  if (!response_obj_.isSuccessCode() ||
      (parser_.isCompleted() && (!isCgi() || cgi_.isCompleted()))) {
    return true;
  }
  return false;
}

void Client::logAddressInfo() const {
  std::cout << "[Client address]" << '\n'
            << cli_address_.getIP() << ":" << cli_address_.getPort() << '\n'
            << "[Server address]" << '\n'
            << serv_address_.getIP() << ":" << serv_address_.getPort() << '\n';
}

void Client::logConnectionInfo() const {
  Log::header("Connection Information");
  logAddressInfo();
  Log::footer();
}

void Client::logReceiveInfo(const std::string& request) const {
  Log::header("Receive Information");
  logAddressInfo();
  std::cout << "[Receive data]" << '\n' << request << '\n';
  Log::footer();
}

const char* Client::ConnectionClosedException::what() const throw() {
  return "";
}