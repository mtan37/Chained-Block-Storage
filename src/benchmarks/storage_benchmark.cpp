#include <iostream>

#include <cstring>

#include "storage.hpp"
#include "timing.h"

void sequential_write() {
  unsigned long nanos;
  Storage::open_volume("sequential.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  long offsets[2];
  DO_TRIALS(++offset;, Storage::write(data, offset, offsets, offset+1);, 100000, nanos);
  print_result("Sequential write", nanos);
  Storage::close_volume();
}

int main(int argc, char** argv) {
  sequential_write();
}
