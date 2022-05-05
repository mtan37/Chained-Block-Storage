#include <cstring>
#include <cstdio>

#include "crash.hpp"
#include "server.h"

static const char indicator[] = "CRASH";

extern std::string my_ip;
extern int my_port;

union crash_code {
  long code;
  struct {
    char chars[sizeof(indicator) - 1];
    unsigned char server_num;
    unsigned short crash_point;
  };
};

static unsigned char get_server_num(std::string ip) {
  if (ip == "") {
    return 0;
  }
  unsigned char dummy[4];
  if (sscanf(ip.c_str(), "%hhu.%hhu.%hhu.%hhu", dummy, dummy+1, dummy+2, dummy+3) != 4) {
    return 0;
  }
  return dummy[3];
}

namespace Crash {

long construct(std::string ip, unsigned short crash_point) {
  crash_code code;
  strncpy(code.chars, indicator, sizeof(indicator) - 1);
  code.server_num = get_server_num(ip);
  code.crash_point = crash_point;
  return code.code;
}

long check(long offset, unsigned short crash_point) {
  crash_code code;
  code.code = offset;
  if (strncmp(code.chars, indicator, sizeof(indicator) - 1) == 0) {
    unsigned char server_num = get_server_num(server::my_ip);
    if (server_num == code.server_num && crash_point == code.crash_point) {
      assert(0);
    }
    return 0;
  }
  return offset;
}

}; // namespace Crash
