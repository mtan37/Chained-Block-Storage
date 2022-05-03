#include <iostream>

#include <cassert>
#include <cstring>

#include "storage.hpp"

void test1() {
  char data[4096];
  
  long seq_num = Storage::get_sequence_number();

  //test unaligned writes at beginning
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data, sizeof(data), "%15d", i);

    long offset = i*16;
    Storage::write(data, offset, ++seq_num);
    
    Storage::commit(seq_num, offset);
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
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data + 4096 - 16, 16, "%15d", i);

    long offset = 4096 - i*16;
    Storage::write(data, offset, ++seq_num);

    Storage::commit(seq_num, offset);
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
  long offset = (4096*256*512) - 2048;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(data + 16*i, 16, "%15d", i);
  }
  Storage::write(data, offset, ++seq_num);
  Storage::commit(seq_num, offset);
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
  long offset = 8*4096;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(data + 16*i, 16, "%15d", i);
  }
  Storage::write(data, offset, ++seq_num);
  Storage::read(data, offset);
  for (int i = 0; i < 4096/16; ++i) {
    char* actual = data + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
  Storage::commit(seq_num, offset);

}

void test5() {
  char data[4096];
  
  long seq_num = Storage::get_sequence_number();

  //like test1 but do all writes bofore comitting
  long sequence_numbers[32];
  for (int i = 0; i < 32; ++i) {
    memset(data, 0, sizeof(data));
    snprintf(data, sizeof(data), "%15d", i);

    long offset = 8*4096 + i*16;
    Storage::write(data, offset, ++seq_num);
    sequence_numbers[i] = seq_num;
  }
  for (int i = 0; i < 32; ++i) {
    seq_num = sequence_numbers[i];
    long offset = 8*4096 + i*16;
    Storage::commit(seq_num, offset);
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
  long offset = 0;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(in.data() + 16*i, 16, "%15d", i);
  }
  Storage::write(in, offset, ++seq_num);
  Storage::commit(seq_num, offset);
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
  long offset = 0;
  for (int i = 0; i < 4096/16; ++i) {
    snprintf(in.data() + 16*i, 16, "%15d", i);
  }
  Storage::write(in, offset, ++seq_num);
  Storage::commit(seq_num, offset);
  Storage::read(out, offset);
  for (int i = 0; i < 4096/16; ++i) {
    char* actual = out.data() + 16*i;
    char expected[16];
    snprintf(expected, sizeof(expected), "%15d", i);
    assert(strcmp(actual, expected) == 0);
  }
}

void test8() {
  std::string checksum = Storage::checksum();
  Storage::close_volume();
  Storage::open_volume("storageTest.volume");
  std::string checksum2 = Storage::checksum();
  assert(checksum == checksum2);
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
  //test8();
  
  std::cout << "All storage tests passed" << std::endl;
  return 0;
}
