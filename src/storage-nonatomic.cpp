#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <cmath>
#include <cstring>

#include <fcntl.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <unistd.h>

#include "storage.hpp"


#define BLOCK_SIZE 4096
#define VOLUME_SIZE 256*1024*1024*1024l 
#define NUM_BLOCKS (VOLUME_SIZE/BLOCK_SIZE)
#define METADATA_SIZE (NUM_BLOCKS * 16)

struct metadata_entry {
  int64_t last_updated;
  int64_t file_block_num;
};

struct uncommitted_write {
  std::vector<int64_t> to_free;
  int64_t new_file_offset[2];
};

static std::unordered_map<int64_t, uncommitted_write*> uncommitted_writes; 
static std::unordered_map<int64_t, int64_t> pending_blocks;

static std::queue<int64_t> free_blocks;

static int fd;

static int num_total_blocks;

static void init_storage(const char* filename) {
  fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    perror("open");
    exit(1);
  }
}

static void write_block(const void* buf, long file_block_num) {
  if(pwrite(fd, buf, BLOCK_SIZE, file_block_num * BLOCK_SIZE) == -1) {
    perror("write");
    exit(1);
  }
}

static void read_block(void* buf, long file_block_num) {
  if(pread(fd, buf, BLOCK_SIZE, file_block_num * BLOCK_SIZE) == -1) {
    perror("read");
    exit(1);
  }
}

//write metadata for alligned volume_offset
static void write_metadata(metadata_entry* buf, long volume_offset) {
  int file_offset = volume_offset / BLOCK_SIZE * sizeof(metadata_entry) + BLOCK_SIZE;
  if (pwrite(fd, buf, sizeof(metadata_entry), file_offset) == -1) {
    perror("write");
    exit(1);
  }
}

//read metadata for alligned volume_offset
static void read_metadata(metadata_entry* buf, long volume_offset) {
  int file_offset = volume_offset / BLOCK_SIZE * sizeof(metadata_entry) + BLOCK_SIZE;
  if (pread(fd, buf, sizeof(metadata_entry), file_offset) == -1) {
    perror("read");
    std::cout << volume_offset << std::endl;
    exit(1);
  }
}

static void set_sequence_num(long sequence_num) {
  if (pwrite(fd, &sequence_num, sizeof(sequence_num), 0) == -1) {
    perror("write");
    exit(1);
  }
}

static long read_sequence_num() {
  long sequence_num;
  if (pread(fd, &sequence_num, sizeof(sequence_num), 0) == -1) {
    perror("read");
    exit(1);
  }
  return sequence_num;
}

static void init_metadata() {
  char buf[BLOCK_SIZE];
  memset(buf, 0, BLOCK_SIZE);
  for (int i = 0; i < METADATA_SIZE/BLOCK_SIZE; ++i) {
    write_block(buf, i + 1);
  }
}

static void read_aligned(char* buf, long volume_offset) {
  if (pending_blocks.count(volume_offset) != 0) {
    read_block(buf, pending_blocks[volume_offset]);
    return;
  }
  
  metadata_entry entry;
  read_metadata(&entry, volume_offset);
  if (entry.file_block_num) {
    read_block(buf, entry.file_block_num);
  }
  else {
    memset(buf, 0, BLOCK_SIZE);
  }
}

static int64_t get_free_block_num() {
  if (free_blocks.size() == 0) {
    ++num_total_blocks;
    return num_total_blocks;
  }
  int64_t num = free_blocks.front();
  free_blocks.pop();
  return num;
}

namespace Storage {

void open_volume(std::string volume_name) {
  init_storage(volume_name.c_str());

  off_t res = lseek(fd, 0, SEEK_END);
  if (res == -1) {
    perror("lseek");
    exit(1);
  }
  num_total_blocks = (res + 1) / BLOCK_SIZE;
  
  if (num_total_blocks == 0) {
    init_metadata();
    set_sequence_num(0l);
    num_total_blocks = 1 + METADATA_SIZE/BLOCK_SIZE;
  }

  std::vector<int64_t> used_blocks;

  // figure out which blocks are in use
  for (int i = 0; i < NUM_BLOCKS; ++i) {
    metadata_entry entry;
    read_metadata(&entry, i);
    if (entry.file_block_num) {
      used_blocks.push_back(entry.file_block_num);
    }
  }
  
  std::sort(used_blocks.begin(), used_blocks.end());

  // Add unused blocks to the free list
  int used_idx = 0;
  int block_num = 1 + METADATA_SIZE;

  while (block_num < num_total_blocks) {
    if (used_idx < used_blocks.size() && block_num == used_blocks[used_idx]) {
      ++used_idx;
    }
    else {
      free_blocks.push(block_num);
    }
    ++block_num;
  }
}

void init_volume(std::string volume_name) {
  init_storage(volume_name.c_str());
  
  init_metadata();
  set_sequence_num(0l);
  num_total_blocks = 1 + METADATA_SIZE/BLOCK_SIZE;
  
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) {
    perror("fstat");
    exit(1);
  } 
  int64_t blocks_in_file = statbuf.st_size / BLOCK_SIZE;
  for (int i = num_total_blocks; i < blocks_in_file; ++i) {
    free_blocks.push(i);
  }
}

void close_volume() {
  fsync(fd);
  close(fd);
}

void write(std::string buf, long volume_offset, long sequence_number) {
  write(buf.data(), volume_offset, sequence_number);
}

void write(std::vector<char> buf, long volume_offset, long sequence_number) {
  write(&buf[0], volume_offset, sequence_number);
}

void write(const char* buf, long volume_offset, long sequence_number) {
  int64_t remainder = volume_offset % BLOCK_SIZE;

  uncommitted_write* uw = new uncommitted_write;
  
  if (remainder) {
    int64_t offset = volume_offset - remainder;
    metadata_entry entry;
    char buf1[BLOCK_SIZE];

    int64_t block_num = get_free_block_num();
    read_metadata(&entry, offset);
    read(buf1, offset);
    memcpy(buf1 + remainder, buf, BLOCK_SIZE - remainder);
    write_block(buf1, block_num);
    uw->new_file_offset[0] = block_num;
    pending_blocks[offset] = block_num;
    if (entry.file_block_num) {
      uw->to_free.push_back(entry.file_block_num);
    }

    offset += BLOCK_SIZE;
    block_num = get_free_block_num();
    read_metadata(&entry, offset);
    read(buf1, offset);
    memcpy(buf1, buf + BLOCK_SIZE - remainder, remainder);
    write_block(buf1, block_num);
    uw->new_file_offset[1] = block_num;
    pending_blocks[offset] = block_num;
    if (entry.file_block_num) {
      uw->to_free.push_back(entry.file_block_num);
    }
  }
  else {
    metadata_entry entry;
    int64_t block_num = get_free_block_num();
    read_metadata(&entry, volume_offset);
    write_block(buf, block_num);
    uw->new_file_offset[0] = block_num;
    uw->new_file_offset[1] = 0;
    pending_blocks[volume_offset] = block_num;
    if (entry.file_block_num) {
      uw->to_free.push_back(entry.file_block_num);
    }
  }

  uncommitted_writes[sequence_number] = uw;
}

void read(std::string& buf, long volume_offset) {
  buf.resize(BLOCK_SIZE);
  read(buf.data(), volume_offset);
}

void read(std::vector<char>& buf, long volume_offset) {
  buf.resize(BLOCK_SIZE);
  read(&buf[0], volume_offset);
}

void read(char* buf, long volume_offset) {
  int64_t remainder = volume_offset % BLOCK_SIZE;

  if (remainder) {
    int64_t offset = volume_offset - remainder;
    char buf1[BLOCK_SIZE];

    read_aligned(buf1, offset);
    memcpy(buf, buf1 + remainder, BLOCK_SIZE - remainder);

    offset += BLOCK_SIZE;
    read_aligned(buf1, offset);
    memcpy(buf + BLOCK_SIZE - remainder, buf1, remainder);
  }
  else {
    read_aligned(buf, volume_offset);
  }
}

bool read_sequence_number(std::string& buf, long seq_num, long volume_offset) {
  buf.resize(BLOCK_SIZE);
  return read_sequence_number(buf.data(), seq_num, volume_offset);
}

bool read_sequence_number(std::vector<char>& buf, long seq_num, long volume_offset) {
  buf.resize(BLOCK_SIZE);
  return read_sequence_number(&buf[0], seq_num, volume_offset);
}

bool read_sequence_number(char *buf, long seq_num, long volume_offset) {
  uncommitted_write* uw = uncommitted_writes[seq_num];
  if (uw == nullptr) {
    return false;
  }

  int64_t remainder = volume_offset % BLOCK_SIZE;

  if (remainder) {
    int64_t offset = volume_offset - remainder;
    char buf1[BLOCK_SIZE];
    read_block(buf1, uw->new_file_offset[0]);
    memcpy(buf, buf1 + remainder, BLOCK_SIZE - remainder);

    offset += BLOCK_SIZE;
    read_block(buf1, uw->new_file_offset[1]);
    memcpy(buf + BLOCK_SIZE - remainder, buf1, remainder);
  }
  else {
    read_block(buf, uw->new_file_offset[0]);
  }
  return true;
}

void commit(long sequence_number, long volume_offset) {
  uncommitted_write* uw = uncommitted_writes[sequence_number];
  uncommitted_writes.erase(sequence_number);

  int64_t remainder = volume_offset % BLOCK_SIZE;
  volume_offset -= remainder;
  
  pending_blocks.erase(volume_offset);
  metadata_entry entry = {sequence_number, uw->new_file_offset[0]};
  write_metadata(&entry, volume_offset);

  if (uw->new_file_offset[1] != 0 && uw->new_file_offset[0] != uw->new_file_offset[1]) {
    volume_offset += 4096;  
    pending_blocks.erase(volume_offset);
    entry = {sequence_number, uw->new_file_offset[1]};
    write_metadata(&entry, volume_offset);
  }

  // fsync twice to make sure that the data and metadata are persistent before updating sequence number
  if (fdatasync(fd) == -1) {
    perror("fsync");
    exit(1);
  }
  set_sequence_num(sequence_number);
  if (fdatasync(fd) == -1) {
    perror("fsync");
    exit(1);
  }

  for (int64_t num : uw->to_free) {
    free_blocks.push(num);
  }

  delete uw;
}

long get_sequence_number() {
  return read_sequence_num();
}

std::string checksum() {
  char data[BLOCK_SIZE];
  int results_size = NUM_BLOCKS * MD5_DIGEST_LENGTH;
  unsigned char* results = new unsigned char[results_size];

  for (long i = 0; i < NUM_BLOCKS; ++i) {
    read_aligned(data, i*BLOCK_SIZE);
    MD5((unsigned char*) data, BLOCK_SIZE, results + i*MD5_DIGEST_LENGTH);
  }

  unsigned char final_result[MD5_DIGEST_LENGTH];

  MD5(results, results_size, final_result);

  std::string to_return;
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
    ss << results[i];
  }
  ss >> to_return;

  delete[] results;

  return to_return;
}

} // namespace Storage
