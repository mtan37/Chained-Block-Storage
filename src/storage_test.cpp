#include <iostream>

#include <cassert>
#include <cstring>

#include "storage.hpp"

int main(int argc, char** argv) {
  Storage::open_volume("storageTest.volume");
  char data[4096];
  
  long seq_num = Storage::get_sequence_number() + 1;

  //test unaligned writes at beginning
  long offsets[2];
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data, sizeof(data), "%15d", i);

    long offset = i*16;
    Storage::write(data, offset, offsets, seq_num++);
    
    Storage::commit(seq_num, offsets, offset);
  }
  Storage::read(data, 0);
  for (int i = 0; i < 32; ++i) {
    char* actual = data + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
  
  //test unaligned writes at end
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data + 4096 - 16, 16, "%15d", i);

    long offset = 4096 - i*16;
    Storage::write(data, offset, offsets, seq_num++);

    Storage::commit(seq_num, offsets, offset);
  }
  Storage::read(data, 4096);
  for (int i = 0; i < 32; ++i) {
    char* actual = data + 4096 - 16*(i+1);
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
  
  std::cout << "All storage tests passed" << std::endl;
  return 0;
}
