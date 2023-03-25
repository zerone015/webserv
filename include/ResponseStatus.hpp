#ifndef RESPONSE_STATUS_HPP_
#define RESPONSE_STATUS_HPP_

#include <string>

enum StatusIndex {
  C200,
  C400,
  C404,
  C405,
  C411,
  C413,
  C500,
  C501,
  C505,
};

struct ResponseStatus {
  static const std::string CODES[];
  static const std::string REASONS[];
};

#endif
