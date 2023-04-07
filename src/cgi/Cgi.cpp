#include "Cgi.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ResponseStatus.hpp"
#include "constant.hpp"
#include "exception.hpp"

Cgi::Cgi() : is_completed_(false) {}

Cgi::Cgi(const Cgi& src)
    : is_completed_(src.is_completed_),
      body_(src.body_),
      response_(src.response_),
      pid_(src.pid_),
      timeout_(src.timeout_) {
  pipe_fds_[READ] = src.pipe_fds_[READ];
  pipe_fds_[WRITE] = src.pipe_fds_[WRITE];
}

Cgi& Cgi::operator=(const Cgi& src) {
  if (this != &src) {
    is_completed_ = src.is_completed_;
    body_ = src.body_;
    response_ = src.response_;
    pid_ = src.pid_;
    timeout_ = src.timeout_;
    pipe_fds_[READ] = src.pipe_fds_[READ];
    pipe_fds_[WRITE] = src.pipe_fds_[WRITE];
  }
  return *this;
}

Cgi::~Cgi() {}

void Cgi::execute(const HttpRequest& request_obj,
                  const HttpResponse& response_obj,
                  const SocketAddress& cli_addr,
                  const SocketAddress& serv_addr) {
  int pipe_fds[2][2];

  std::string cgi_path =
      response_obj.getLocationBlock()->getCgiParam("CGI_PATH");
  std::string uri = getAbsolutePath(request_obj.getUri());

  if (pipe(pipe_fds[0]) == ERROR<int>()) {
    throw ResponseException(C500);
  }
  if (pipe(pipe_fds[1]) == ERROR<int>()) {
    close(pipe_fds[0][READ]);
    close(pipe_fds[0][WRITE]);
    throw ResponseException(C500);
  }

  char* argv[3] = {const_cast<char*>(cgi_path.c_str()),
                   const_cast<char*>(uri.c_str()), NULL};
  char** envp = generateEnvp(request_obj, response_obj, cli_addr, serv_addr);

  pid_ = fork();

  if (pid_ == ERROR<pid_t>()) {
    deleteEnvp(envp);
    close(pipe_fds[0][READ]);
    close(pipe_fds[0][WRITE]);
    close(pipe_fds[1][READ]);
    close(pipe_fds[1][WRITE]);
    throw ResponseException(C500);
  } else if (pid_ == 0) {
    close(pipe_fds[0][WRITE]);
    close(pipe_fds[1][READ]);
    dup2(pipe_fds[0][READ], STDIN_FILENO);
    dup2(pipe_fds[1][WRITE], STDOUT_FILENO);

    execve(argv[0], argv, envp);
    exit(EXIT_FAILURE);
  }

  deleteEnvp(envp);
  close(pipe_fds[0][READ]);
  close(pipe_fds[1][WRITE]);

  pipe_fds_[READ] = pipe_fds[1][READ];
  pipe_fds_[WRITE] = pipe_fds[0][WRITE];

  if (fcntl(pipe_fds_[READ], F_SETFL, O_NONBLOCK) == ERROR<int>() ||
      fcntl(pipe_fds_[WRITE], F_SETFL, O_NONBLOCK) == ERROR<int>()) {
    kill(pid_, SIGKILL);
    close(pipe_fds_[READ]);
    close(pipe_fds_[WRITE]);
    throw ResponseException(C500);
  }

  if (request_obj.getMethod() != "POST") {
    close(pipe_fds_[WRITE]);
  }
  body_ = request_obj.getBody();
  timeout_ = std::time(NULL) + CGI_TIMEOUT;
}

void Cgi::write(Selector& selector) {
  std::size_t write_bytes =
      ::write(pipe_fds_[WRITE], body_.c_str(), body_.size());

  if (write_bytes == ERROR<std::size_t>()) {
    kill(pid_, SIGKILL);
    close(pipe_fds_[READ]);
    close(pipe_fds_[WRITE]);
    selector.unregisterFD(pipe_fds_[READ]);
    selector.unregisterFD(pipe_fds_[WRITE]);
    throw ResponseException(C500);
  }
  if (write_bytes == body_.size()) {
    close(pipe_fds_[WRITE]);
    selector.unregisterFD(pipe_fds_[WRITE]);
  }
  body_.erase(0, write_bytes);
}

void Cgi::read(Selector& selector) {
  char buf[BUF_SIZE];

  std::size_t read_bytes = ::read(pipe_fds_[READ], buf, BUF_SIZE);

  if (read_bytes == ERROR<std::size_t>()) {
    kill(pid_, SIGKILL);
    close(pipe_fds_[READ]);
    close(pipe_fds_[WRITE]);
    selector.unregisterFD(pipe_fds_[READ]);
    selector.unregisterFD(pipe_fds_[WRITE]);
    throw ResponseException(C500);
  }
  if (read_bytes == 0) {
    is_completed_ = true;
    close(pipe_fds_[READ]);
    selector.unregisterFD(pipe_fds_[READ]);
  }
  response_ += std::string(buf, read_bytes);
  timeout_ = std::time(NULL) + CGI_TIMEOUT;
}

const std::string& Cgi::getResponse() const { return response_; }

int Cgi::getWriteFD() const { return pipe_fds_[WRITE]; }

int Cgi::getReadFD() const { return pipe_fds_[READ]; }

std::time_t Cgi::getTimeout() const { return timeout_;}

bool Cgi::isCompleted() const { return is_completed_; }

bool Cgi::hasBody() const { return !body_.empty(); }

void Cgi::clear() {
  is_completed_ = false;
  response_.clear();
}

void Cgi::cleanUp(Selector& selector) const
{
  kill(pid_, SIGKILL);
  if (pipe_fds_[READ] != -1) {
    close(pipe_fds_[READ]);
    selector.unregisterFD(pipe_fds_[READ]);
  }
  if (pipe_fds_[WRITE] != -1) {
    close(pipe_fds_[WRITE]);
    selector.unregisterFD(pipe_fds_[WRITE]);
  }
}

char** Cgi::generateEnvp(const HttpRequest& request_obj,
                         const HttpResponse& response_obj,
                         const SocketAddress& cli_addr,
                         const SocketAddress& serv_addr) const {
  std::map<std::string, std::string> env_map;

  const std::string& method = request_obj.getMethod();
  std::size_t content_length = request_obj.getContentLength();
  if (method == "POST" && content_length > 0) {
    env_map["CONTENT_LENGTH"] = toString(content_length);
  }
  env_map["AUTH_TYPE"] = "";
  env_map["CONTENT_TYPE"] = request_obj.getHeader("CONTENT-TYPE");
  env_map["GATEWAY_INTERFACE"] = "CGI/1.1";
  env_map["PATH_INFO"] = request_obj.getUri();
  env_map["PATH_TRANSLATED"] = getAbsolutePath(request_obj.getUri());
  env_map["QUERY_STRING"] = request_obj.getQueryString();
  env_map["REMOTE_HOST"] = "";
  env_map["REMOTE_ADDR"] = cli_addr.getIP();
  env_map["REMOTE_USER"] = "";
  env_map["REMOTE_IDENT"] = "";
  env_map["REQUEST_METHOD"] = method;
  env_map["REQUEST_URI"] = request_obj.getUri();
  env_map["SCRIPT_NAME"] = request_obj.getUri();
  env_map["SERVER_NAME"] = serv_addr.getIP();
  env_map["SERVER_PROTOCOL"] = "HTTP/1.1";
  env_map["SERVER_PORT"] = serv_addr.getPort();
  env_map["SERVER_SOFTWARE"] = "webserv/1.1";
  env_map["HTTP_X_SERVER_KEY"] =
      toString(response_obj.getServerBlock()->getKey());
  Session* session = response_obj.getSession();
  if (session) {
    env_map["HTTP_X_SESSION_ID"] = session->getID();
  }
  env_map["HTTP_X_SECRET_HEADER_FOR_TEST"] =
      request_obj.getHeader("X-SECRET-HEADER-FOR-TEST");

  char** envp = new char*[env_map.size() + 1];

  int i = 0;
  for (std::map<std::string, std::string>::const_iterator it = env_map.begin();
       it != env_map.end(); ++it, ++i) {
    envp[i] = strdup((it->first + "=" + it->second).c_str());
    if (envp[i] == NULL) {
      deleteEnvp(envp);
      throw ResponseException(C500);
    }
  }
  envp[i] = NULL;
  return envp;
}

void Cgi::deleteEnvp(char** envp) const {
  for (int i = 0; envp[i]; ++i) {
    delete[] envp[i];
  }
  delete[] envp;
}

std::string Cgi::getAbsolutePath(const std::string& uri) const {
  char buf[FILENAME_MAX];
  if (!getcwd(buf, FILENAME_MAX)) {
    throw ResponseException(C500);
  }
  return buf + uri;
}
