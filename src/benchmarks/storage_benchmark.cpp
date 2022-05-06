#include <iostream>

#include <cstring>

#include "storage.hpp"
#include "timing.h"

void sequential_write() {
  unsigned long nanos;
  Storage::init_volume("sequential.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, Storage::write(data, offset, offset+1);, 100000, nanos);
  print_result("Sequential write", nanos);
  Storage::close_volume();
}

void sequential_write_commit() {
  unsigned long nanos;
  Storage::init_volume("sequential.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, {
    Storage::write(data, offset, offset+1); 
    Storage::commit(offset+1, offset);
  }, 10000, nanos);
  print_result("Sequential write-commit", nanos);
  Storage::close_volume();
}

void sequential_read() {
  unsigned long nanos;
  Storage::open_volume("sequential.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, {
    Storage::read(data, offset); 
  }, 10000, nanos);
  print_result("Sequential read", nanos);
  Storage::close_volume();
}

void sequential_commit() {
  unsigned long nanos;
  Storage::init_volume("sequential.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  while (++offset < 10000) { 
    Storage::write(data, offset, offset+1); 
  }
  offset = -1;
  DO_TRIALS({
    ++offset;
  }, {
    Storage::commit(offset+1, offset);
  }, 10000, nanos);
  print_result("Sequential commit", nanos);
  Storage::close_volume();
}

void empty_checksum() {
  unsigned long nanos;
  Storage::init_volume("empty.volume");
  DO_TRIALS(, Storage::checksum();, 10, nanos);
  print_result("Empty checksum", nanos);
  Storage::close_volume();
}

void init_volume() {
  unsigned long nanos;
  Storage::init_volume("empty.volume");
  DO_TRIALS(Storage::close_volume();, Storage::init_volume("empty.volume");, 10, nanos);
  print_result("Init volume", nanos);
  Storage::close_volume();
}

int main(int argc, char** argv) {
  std::cout << "Using storage system: " << Storage::get_storage_type() << std::endl;
  sequential_write();
  sequential_write_commit();
  sequential_commit();
  sequential_read();
  empty_checksum();
  init_volume();
}
