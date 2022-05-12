#include <algorithm>
#include <iostream>
#include <random>

#include <cstring>

#include <fcntl.h>
#include <unistd.h>

#include "storage.hpp"
#include "timing.h"

#define SEQ_VOL "sequential.volume"
#define RAN_VOL "random.voluume"
#define USEQ_VOL "usequential.volume"
#define URAN_VOL "urandom.volume"

#define TRIALS 10000

std::vector<long> seq_order;
std::vector<long> ran_order;

unsigned long test_write(std::string volume, std::vector<long>& order, long diff) {
  unsigned long nanos;
  Storage::init_volume(volume);
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, Storage::write(data, order[offset]*4096 + diff, offset+1);, TRIALS, nanos);
  return nanos;
}

void sequential_write() {
  print_result("Sequential write", test_write(SEQ_VOL, seq_order, 0));
}

void random_write() {
  print_result("Random write", test_write(RAN_VOL, ran_order, 0));
}

void unaligned_sequential_write() {
  print_result("Unalligned sequential write", test_write(USEQ_VOL, seq_order, 2048));
}

void unaligned_random_write() {
  print_result("Unalligned random write", test_write(URAN_VOL, ran_order, 2048));
}

unsigned long test_commit(std::string volume, std::vector<long>& order, long diff) {
  unsigned long nanos;
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  offset = -1;
  DO_TRIALS({
    ++offset;
  }, {
    Storage::commit(offset+1, order[offset]*4096 + diff);
  }, TRIALS, nanos);
  return nanos;
}

void sequential_commit() {
  print_result("Sequential commit", test_commit(SEQ_VOL, seq_order, 0));
}

void random_commit() {
  print_result("Random commit", test_commit(RAN_VOL, ran_order, 0));
}

void unaligned_sequential_commit() {
  print_result("Unalligned sequential commit", test_commit(USEQ_VOL, seq_order, 2048));
}

void unaligned_random_commit() {
  print_result("Unalligned random commit", test_commit(URAN_VOL, ran_order, 2048));
}

unsigned long test_read(std::vector<long>& order, long diff) {
  unsigned long nanos;
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, {
    Storage::read(data, order[offset]*4096 + diff); 
  }, TRIALS, nanos);
  Storage::close_volume();
  return nanos;
}

void sequential_read() {
  print_result("Sequential read", test_read(seq_order, 0));
}

void random_read() {
  print_result("Random read", test_read(ran_order, 0));
}

void unaligned_sequential_read() {
  print_result("Unalgined sequential read", test_read(seq_order, 2048));
}

void unaligned_random_read() {
  print_result("Unalgined random read", test_read(ran_order, 2048));
}

unsigned long test_write_commit(std::string volume, std::vector<long>& order, long diff) {
  unsigned long nanos;
  Storage::init_volume(volume);
  char data[4096];
  memset(data, 1, sizeof(data));
  int offset = -1;
  DO_TRIALS(++offset;, {
    Storage::write(data, order[offset]*4096 + diff, offset+1); 
    Storage::commit(offset+1, order[offset]*4096 + diff);
  }, TRIALS, nanos);
  Storage::close_volume();
  return nanos;
}

void sequential_write_commit() {
  print_result("Sequential write-commit", test_write_commit("test1.volume", seq_order, 0));
}

void random_write_commit() {
  print_result("Random write-commit", test_write_commit("test2.volume", ran_order, 0));
}

void unaligned_sequential_write_commit() {
  print_result("Unaligned sequential write-commit", test_write_commit("test3.volume", seq_order, 2048));
}

void unaligned_random_write_commit() {
  print_result("Unaligned random write-commit", test_write_commit("test4.volume", ran_order, 2048));
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
  for (int i = 0; i < 100000; ++i) {
    seq_order.push_back(i);
    ran_order.push_back(i);
  }
  std::shuffle(ran_order.begin(), ran_order.end(), std::default_random_engine());

  unlink(SEQ_VOL);
  unlink(RAN_VOL);
  unlink(USEQ_VOL);
  unlink(URAN_VOL);
  unlink("test1.volume");
  unlink("test2.volume");
  unlink("test3.volume");
  unlink("test4.volume");
  unlink("empty.volume");
  int fd = open(".", O_RDONLY);
  fsync(fd);
  close(fd);

  //empty_checksum();
  //init_volume();

  sequential_write();
  sequential_commit();
  sequential_read();
  random_write();
  random_commit();
  random_read();
  unaligned_sequential_write();
  unaligned_sequential_commit();
  unaligned_sequential_read();
  unaligned_random_write();
  unaligned_random_commit();
  unaligned_random_read();

  sequential_write_commit();
  random_write_commit();
  unaligned_sequential_write_commit();
  unaligned_random_write_commit();
}
