#include <iostream>

#include <string.h>

#include "storage.hpp"

int main(int argc, char** argv) {
  Storage::open_volume("storageTest.volume");
  char data[4096];
  
  long seq_num = Storage::get_sequence_number() + 1;

  long offsets[2];
  for (int i = 0; i < 20; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data, sizeof(data), "%16d", i);

    std:: cout << data << std::endl;
    long offset = i*16;
    Storage::write(data, offset, offsets);
  
    std::cout << offsets[0] << " " << offsets[1] << std::endl;

    Storage::commit(seq_num + i, offsets, offset);
  }
  return 0;
}
