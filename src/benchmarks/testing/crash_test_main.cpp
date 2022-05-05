#include <iostream>

#include "crash.hpp"
#include "test.h"

int main() {
  long code = Crash::construct("", 1);
  Crash::check(code, 0);
  std::cout << "Passed first check" << std::endl;
  Crash::check(code, 1);
  std::cout << "Should have crashed" << std::endl;
}
