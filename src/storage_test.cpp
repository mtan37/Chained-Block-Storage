#include <iostream>

#include <cassert>
#include <cstring>

#include "storage.hpp"

void test1() {
  char data[4096];
  
  long seq_num = Storage::get_sequence_number();

  //test unaligned writes at beginning
  long offsets[2];
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data, sizeof(data), "%15d", i);

    long offset = i*16;
    Storage::write(data, offset, offsets, ++seq_num);
    
    Storage::commit(seq_num, offsets, offset);
  }
  Storage::read(data, 0);
  for (int i = 0; i < 32; ++i) {
    char* actual = data + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
}

void test2() {
  char data[4096];
  
  long seq_num = Storage::get_sequence_number();

  //test unaligned writes at end
  long offsets[2];
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data + 4096 - 16, 16, "%15d", i);

    long offset = 4096 - i*16;
    Storage::write(data, offset, offsets, ++seq_num);

    Storage::commit(seq_num, offsets, offset);
  }
  Storage::read(data, 4096);
  for (int i = 0; i < 32; ++i) {
    char* actual = data + 4096 - 16*(i+1);
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
}

void test3() {
  char data[4096];
  
  long seq_num = Storage::get_sequence_number();

  //test unaligned write that crosses metadata border
  long offsets[2];
  long offset = (4096*256*512) - 2048;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(data + 16*i, 16, "%15d", i);
  }
  Storage::write(data, offset, offsets, ++seq_num);
  Storage::commit(seq_num, offsets, offset);
  Storage::read(data, offset);
  for (int i = 0; i < 4096/16; ++i) {
    char* actual = data + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
}

void test4() {
  char data[4096];
  
  long seq_num = Storage::get_sequence_number();

  //test aligned read after write but no commit
  long offsets[2];
  long offset = 8*4096;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(data + 16*i, 16, "%15d", i);
  }
  Storage::write(data, offset, offsets, ++seq_num);
  Storage::read(data, offset);
  for (int i = 0; i < 4096/16; ++i) {
    char* actual = data + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
  Storage::commit(seq_num, offsets, offset);

}

void test5() {
  char data[4096];
  
  long seq_num = Storage::get_sequence_number();

  //like test1 but do all writes bofore comitting
  long offsets[32][2];
  long sequence_numbers[32];
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data, sizeof(data), "%15d", i);

    long offset = 8*4096 + i*16;
    Storage::write(data, offset, offsets[i], ++seq_num);
    sequence_numbers[i] = seq_num;
  }
  for (int i = 0; i < 32; ++i) {
    seq_num = sequence_numbers[i];
    long offset = 8*4096 + i*16;
    Storage::commit(seq_num, offsets[i], offset);
  }
  Storage::read(data, 8*4096);
  for (int i = 0; i < 32; ++i) {
    char* actual = data + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
}

void test6() {
  std::string in, out;
  in.resize(4096);
  long seq_num = Storage::get_sequence_number();
 
  // test that read/write with c++ strings work
  long offsets[2];
  long offset = 0;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(in.data() + 16*i, 16, "%15d", i);
  }
  Storage::write(in, offset, offsets, ++seq_num);
  Storage::commit(seq_num, offsets, offset);
  Storage::read(out, offset);
  for (int i = 0; i < 4096/16; ++i) {
    char* actual = out.data() + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
}

void test7() {
  std::vector<char> in, out;
  in.resize(4096);
  long seq_num = Storage::get_sequence_number();
 
  // test that read/write with c++ vectors work
  long offsets[2];
  long offset = 0;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(in.data() + 16*i, 16, "%15d", i);
  }
  Storage::write(in, offset, offsets, ++seq_num);
  Storage::commit(seq_num, offsets, offset);
  Storage::read(out, offset);
  for (int i = 0; i < 4096/16; ++i) {
    char* actual = out.data() + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
}

int main(int argc, char** argv) {
  Storage::open_volume("storageTest.volume");

  test1();
  test2();
  test3();
  test4();
  test5();
  test6();
  test7();
  
  std::cout << "All storage tests passed" << std::endl;
  return 0;
}
