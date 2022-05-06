#include <algorithm>
#include <iostream>
#include <random>

#include <cstring>

#include "storage.hpp"
#include "timing.h"

std::vector<long> seq_order;
std::vector<long> ran_order;

unsigned long test_write(std::vector<long>& order, long diff) {
  unsigned long nanos;
  Storage::init_volume("test.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, Storage::write(data, order[offset]*4096 + diff, offset+1);, 100000, nanos);
  Storage::close_volume();
  return nanos;
}

void sequential_write() {
  print_result("Sequential write", test_write(seq_order, 0));
}

void random_write() {
  print_result("Random write", test_write(ran_order, 0));
}

void unaligned_sequential_write() {
  print_result("Unalligned sequential write", test_write(seq_order, 2048));
}

void unaligned_random_write() {
  print_result("Unalligned random write", test_write(ran_order, 2048));
}

unsigned long test_write_commit(std::vector<long>& order, long diff) {
  unsigned long nanos;
  Storage::init_volume("test.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, {
    Storage::write(data, order[offset]*4096 + diff, offset+1); 
    Storage::commit(offset+1, order[offset]*4096 + diff);
  }, 10000, nanos);
  Storage::close_volume();
  return nanos;
}

void sequential_write_commit() {
  print_result("Sequential write-commit", test_write_commit(seq_order, 0));
}

void random_write_commit() {
  print_result("Random write-commit", test_write_commit(ran_order, 0));
}

void unaligned_sequential_write_commit() {
  print_result("Unaligned sequential write-commit", test_write_commit(seq_order, 2048));
}

void unaligned_random_write_commit() {
  print_result("Unaligned random write-commit", test_write_commit(ran_order, 2048));
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
/*
void random_write_commit() {
  unsigned long nanos;
  Storage::init_volume("random.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, {
    Storage::write(data, order[offset]*4096, offset+1); 
    Storage::commit(offset+1, order[offset]*4096);
  }, 10000, nanos);
  print_result("Random write-commit", nanos);
  Storage::close_volume();
}

void random_read() {
  unsigned long nanos;
  Storage::open_volume("random.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, {
    Storage::read(data, order[offset]*4096); 
  }, 10000, nanos);
  print_result("Random read", nanos);
  Storage::close_volume();
}

void random_commit() {
  unsigned long nanos;
  Storage::init_volume("random.volume");
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  while (++offset < 10000) { 
    Storage::write(data, order[offset]*4096, offset+1); 
  }
  offset = -1;
  DO_TRIALS({
    ++offset;
  }, {
    Storage::commit(offset+1, order[offset]*4096);
  }, 10000, nanos);
  print_result("Random commit", nanos);
  Storage::close_volume();
}*/

int main(int argc, char** argv) {
  std::cout << "Using storage system: " << Storage::get_storage_type() << std::endl;
  for (int i = 0; i < 100000; ++i) {
    seq_order.push_back(i);
    ran_order.push_back(i);
  }
  std::shuffle(ran_order.begin(), ran_order.end(), std::default_random_engine());

  //empty_checksum();
  //init_volume();

  //sequential_write();
  //random_write();
  //unaligned_sequential_write();
  //unaligned_random_write();

  //sequential_write_commit();
  random_write_commit();
  unaligned_sequential_write_commit();
  //unaligned_random_write_commit();
/*  sequential_write_commit();
  sequential_commit();
  sequential_read();
  
  random_write();
  random_write_commit();
  random_commit();
  random_read();*/
}
