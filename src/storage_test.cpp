#include <iostream>

#include <string.h>

#include "storage.hpp"

int main(int argc, char** argv) {
  Storage::open_volume("storageTest.volume");
  char data[4096];
  strncpy(data, "Hello World", 4096);
  
  long seq_num = Storage::get_sequence_number() + 1;

  long offsets[2];
  for (int i = 0; i < 1; ++i) {
    Storage::write(data, 1, offsets);

    std::cout << offsets[0] << std::endl;

    Storage::commit(seq_num + i, offsets, 1);
  }
  return 0;
}
