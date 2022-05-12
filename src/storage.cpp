#include <algorithm>
#include <iostream>
#include <mutex>
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
#define NUM_ENTRIES (BLOCK_SIZE / 16)
#define NUM_INDIRECT (BLOCK_SIZE / 8)
#define NUM_FIRST_LEVEL (NUM_BLOCKS / NUM_ENTRIES / NUM_INDIRECT / NUM_INDIRECT)

static struct {
  int64_t last_committed;
  int64_t first_level_blocks[NUM_FIRST_LEVEL];
  char padding[BLOCK_SIZE - sizeof(last_committed) - sizeof(first_level_blocks)];
} first_block;

struct metadata_entry {
  int64_t last_updated;
  int64_t file_block_num;
};

union metadata_block {
  int64_t indirect_block[NUM_INDIRECT];
  metadata_entry last_level_block[NUM_ENTRIES];
};

struct uncommitted_write {
  std::vector<int64_t> to_free;
  int64_t new_metadata_offset[2];
};

static std::unordered_map<int64_t, uncommitted_write*> uncommitted_writes; 
static std::unordered_map<int64_t, int64_t> pending_blocks;

static std::queue<int64_t> free_blocks;

static int fd;

static int num_total_blocks;

static std::mutex storage_mutex;

static int64_t get_first_level(int64_t offset) {
  offset /= NUM_ENTRIES;
  offset /= NUM_INDIRECT;
  offset /= NUM_INDIRECT;
  offset /= BLOCK_SIZE;
  return offset & (NUM_FIRST_LEVEL - 1);
}

static int64_t get_second_level(int64_t offset) {
  offset /= NUM_ENTRIES;
  offset /= NUM_INDIRECT;
  offset /= BLOCK_SIZE;
  return offset & (NUM_INDIRECT - 1);
}

static int64_t get_third_level(int64_t offset) {
  offset /= NUM_ENTRIES;
  offset /= BLOCK_SIZE;
  return offset & (NUM_INDIRECT - 1);
}

static int64_t get_last_level(int64_t offset) {
  offset /= BLOCK_SIZE;
  return offset & (NUM_ENTRIES - 1);
}

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

static void read_aligned(char* buf, long volume_offset) {
  if (pending_blocks.count(volume_offset) != 0) {
    read_block(buf, pending_blocks[volume_offset]);
    return;
  }
  int64_t block_num = first_block.first_level_blocks[get_first_level(volume_offset)];
  if (block_num == 0) {
    memset(buf, 0, BLOCK_SIZE);
    return;
  }
  metadata_block block;
  read_block(&block, block_num);
  
  block_num = block.indirect_block[get_second_level(volume_offset)];
  if (block_num == 0) {
    memset(buf, 0, BLOCK_SIZE);
    return;
  }
  read_block(&block, block_num);

  block_num = block.indirect_block[get_third_level(volume_offset)];
  if (block_num == 0) {
    memset(buf, 0, BLOCK_SIZE);
    return;
  }
  read_block(&block, block_num);

  block_num = block.last_level_block[get_last_level(volume_offset)].file_block_num;
  if (block_num == 0) {
    memset(buf, 0, BLOCK_SIZE);
    return;
  }
  read_block(buf, block_num);
}

static int64_t get_free_block_num() {
  if (free_blocks.size() == 0) {
    return ++num_total_blocks;
  }
  int64_t num = free_blocks.front();
  free_blocks.pop();
  return num;
}

static void write_metadata(uncommitted_write& uw, 
                           int64_t sequence_number,
                           int64_t file_offset[2],
                           int64_t volume_offset) {
  int64_t remainder = volume_offset % BLOCK_SIZE;

  int num_file_blocks = remainder ? 2 : 1; 

  metadata_block block[2][3];
  memset(block, 0, sizeof(block));
  int64_t old_block_nums[][4] = {{0,0,0,0},{0,0,0,0}};
  int64_t offsets[][4] = {{0,0,0,0},{0,0,0,0}};
  int64_t v_offsets[] = {volume_offset - remainder, 
                         volume_offset - remainder + BLOCK_SIZE};

  // read any metadata blocks that already exist
  for (int i = 0; i < num_file_blocks; ++i) {
    offsets[i][0] = get_first_level(v_offsets[i]);
    offsets[i][1] = get_second_level(v_offsets[i]);
    offsets[i][2] = get_third_level(v_offsets[i]);
    offsets[i][3] = get_last_level(v_offsets[i]);
    if (uncommitted_writes[sequence_number-1] != nullptr) {
      old_block_nums[i][0] = uncommitted_writes[sequence_number-1]->new_metadata_offset[i];
    }
    else {
      old_block_nums[i][0] = first_block.first_level_blocks[offsets[i][0]];
    }
    if (old_block_nums[i][0] != 0) {
      read_block(&block[i][0], old_block_nums[i][0]);
      old_block_nums[i][1] = block[i][0].indirect_block[offsets[i][1]];
      if (old_block_nums[i][1] != 0) {
        read_block(&block[i][1], old_block_nums[i][1]);
        old_block_nums[i][2] = block[i][1].indirect_block[offsets[i][2]];
        if (old_block_nums[i][2] != 0) {
          read_block(&block[i][2], old_block_nums[i][2]);
          old_block_nums[i][3] = block[i][2].last_level_block[offsets[i][3]].file_block_num;
        }
      }
    }
  }

  for (int i = 0; i < 4; ++i) {
    if (old_block_nums[0][i] != 0) {
      uw.to_free.push_back(old_block_nums[0][i]);
    }
    if (old_block_nums[1][i] != 0 && old_block_nums[1][i] != old_block_nums[0][i]) {
      uw.to_free.push_back(old_block_nums[1][i]);
    }
  }

  // get new block numbers
  int64_t new_block_nums[2][3];
  for (int j = 0; j < 3; ++j) {
    if (offsets[0][j] == offsets[1][j]) { 
      // It is not possible to have a case where the offsets match but the
      // block nums should be different because of the tree structure and
      // the fact that offsets can differ by at most 1 (mod the number of
      // things per block)
      new_block_nums[0][j] = new_block_nums[1][j] = get_free_block_num();
    }
    else {
      for (int i = 0; i < num_file_blocks; ++i) {
        new_block_nums[i][j] = get_free_block_num();
      }
    }
  }

  // updata last level block(s)
  block[0][2].last_level_block[offsets[0][3]].file_block_num = file_offset[0];
  block[0][2].last_level_block[offsets[0][3]].last_updated = sequence_number;
  if (remainder) {
    int i = (offsets[0][2] == offsets[1][2]) ? 0 : 1;
    block[i][2].last_level_block[offsets[1][3]].file_block_num = file_offset[1];
    block[i][2].last_level_block[offsets[1][3]].last_updated = sequence_number;
  }
  
  // updata indirect block(s)
  for (int i = 1; i >= 0; --i) {
    block[0][i].indirect_block[offsets[0][i+1]] = new_block_nums[0][i+1];
    if (remainder) {
      int j = (offsets[0][i] == offsets[1][i]) ? 0 : 1;
      block[j][i].indirect_block[offsets[1][i+1]] = new_block_nums[1][i+1];
    }
  }

  // write new blocks to disk
  for (int j = 0; j < 3; ++j) {
    write_block(&block[0][j], new_block_nums[0][j]);
    if (offsets[0][j] != offsets[1][j]) {
      write_block(&block[1][j], new_block_nums[1][j]);
    }
  }

  for (int i = 0; i < 2; ++i) {
    uw.new_metadata_offset[i] = new_block_nums[i][0];
  }
}

namespace Storage {

std::string get_storage_type() {
  return "atomic";
}

void open_volume(std::string volume_name) {
  init_storage(volume_name.c_str());

  off_t res = lseek(fd, 0, SEEK_END);
  if (res == -1) {
    perror("lseek");
    exit(1);
  }
  num_total_blocks = (res + 1) / BLOCK_SIZE;
  
  if (num_total_blocks == 0) {
    memset(&first_block, 0, sizeof(first_block));
    write_block(&first_block, 0);
    num_total_blocks = 1;
  }
  else {
    read_block(&first_block, 0);
  }

  std::vector<int64_t> used_blocks;

  // figure out which blocks are in use
  for (int i = 0; i < NUM_FIRST_LEVEL; ++i) {
    int64_t first_block_num = first_block.first_level_blocks[i];
    if (first_block_num != 0) {
      used_blocks.push_back(first_block_num);
      metadata_block first_block;
      read_block(&first_block, first_block_num);

      for (int j = 0; j < NUM_INDIRECT; ++j) {
        int64_t second_block_num = first_block.indirect_block[j];
        if (second_block_num != 0) {
          used_blocks.push_back(second_block_num);
          metadata_block second_block;
          read_block(&second_block, second_block_num);

          for (int k = 0; k < NUM_INDIRECT; ++k) {
            int64_t third_block_num = second_block.indirect_block[k];
            if (third_block_num != 0) {
              used_blocks.push_back(third_block_num);
              metadata_block third_block;
              read_block(&third_block, third_block_num);

              for (int l = 0; l < NUM_ENTRIES; ++l) {
                int64_t data_block_num = third_block.last_level_block[l].file_block_num;
                if (data_block_num != 0) {
                  used_blocks.push_back(data_block_num);
                }
              }
            }
          }
        }
      }
    }
  }


  
  std::sort(used_blocks.begin(), used_blocks.end());

  // Add unused blocks to the free list
  int used_idx = 0;
  int block_num = 1;

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

  memset(&first_block, 0, sizeof(first_block));
  write_block(&first_block, 0);
  num_total_blocks = 1;
  
  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) {
    perror("fstat");
    exit(1);
  } 
  int64_t blocks_in_file = statbuf.st_size / BLOCK_SIZE;
  for (int i = 1; i < blocks_in_file; ++i) {
    free_blocks.push(i);
  }
}

void close_volume() {
  fsync(fd);
  close(fd);
  fd = -1;
  num_total_blocks = 0;
  uncommitted_writes.clear();
  pending_blocks.clear();
  while(free_blocks.size() != 0) {
    free_blocks.pop();
  }
}

void write(std::string buf, long volume_offset, long sequence_number) {
  write(buf.data(), volume_offset, sequence_number);
}

void write(std::vector<char> buf, long volume_offset, long sequence_number) {
  write(&buf[0], volume_offset, sequence_number);
}

void write(const char* buf, long volume_offset, long sequence_number) {
    storage_mutex.lock();
  int64_t remainder = volume_offset % BLOCK_SIZE;

  int64_t file_offset[2];
  
  if (remainder) {
    int64_t offset = volume_offset - remainder;
    char buf1[BLOCK_SIZE];

    int64_t block_num = get_free_block_num();
    read(buf1, offset);
    memcpy(buf1 + remainder, buf, BLOCK_SIZE - remainder);
    write_block(buf1, block_num);
    file_offset[0] = block_num;
    pending_blocks[offset] = block_num;

    offset += BLOCK_SIZE;
    block_num = get_free_block_num();
    read(buf1, offset);
    memcpy(buf1, buf + BLOCK_SIZE - remainder, remainder);
    write_block(buf1, block_num);
    file_offset[1] = block_num;
    pending_blocks[offset] = block_num;
  }
  else {
    int64_t block_num = get_free_block_num();
    write_block(buf, block_num);
    file_offset[0] = block_num;
    file_offset[1] = -1;
    pending_blocks[volume_offset] = block_num;
  }
  
  uncommitted_write* uw = new uncommitted_write;
  write_metadata(*uw, sequence_number, file_offset, volume_offset);
  uncommitted_writes[sequence_number] = uw;
  storage_mutex.unlock();
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
    metadata_block mdb;
    read_block(&mdb, uw->new_metadata_offset[0]);
    read_block(&mdb, mdb.indirect_block[get_second_level(offset)]);
    read_block(&mdb, mdb.indirect_block[get_third_level(offset)]);
    read_block(buf1, mdb.last_level_block[get_last_level(offset)].file_block_num);
    memcpy(buf, buf1 + remainder, BLOCK_SIZE - remainder);

    offset += BLOCK_SIZE;
    read_block(&mdb, uw->new_metadata_offset[1]);
    read_block(&mdb, mdb.indirect_block[get_second_level(offset)]);
    read_block(&mdb, mdb.indirect_block[get_third_level(offset)]);
    read_block(buf1, mdb.last_level_block[get_last_level(offset)].file_block_num);
    memcpy(buf + BLOCK_SIZE - remainder, buf1, remainder);
  }
  else {
    metadata_block mdb;
    read_block(&mdb, uw->new_metadata_offset[0]);
    read_block(&mdb, mdb.indirect_block[get_second_level(volume_offset)]);
    read_block(&mdb, mdb.indirect_block[get_third_level(volume_offset)]);
    read_block(buf, mdb.last_level_block[get_last_level(volume_offset)].file_block_num);
  }
  return true;
}

std::vector<std::pair<long, long>> get_modified_offsets(long seq_num) {
  std::vector<std::pair<long, long>> results;
  
  for (int i = 0; i < NUM_FIRST_LEVEL; ++i) {
    int64_t first_block_num = first_block.first_level_blocks[i];
    if (first_block_num != 0) {
      metadata_block first_block;
      read_block(&first_block, first_block_num);

      for (int j = 0; j < NUM_INDIRECT; ++j) {
        int64_t second_block_num = first_block.indirect_block[j];
        if (second_block_num != 0) {
          metadata_block second_block;
          read_block(&second_block, second_block_num);

          for (int k = 0; k < NUM_INDIRECT; ++k) {
            int64_t third_block_num = second_block.indirect_block[k];
            if (third_block_num != 0) {
              metadata_block third_block;
              read_block(&third_block, third_block_num);

              for (int l = 0; l < NUM_ENTRIES; ++l) {
                int64_t volume_offset = BLOCK_SIZE*(l + NUM_ENTRIES*(k + NUM_INDIRECT*(j + NUM_INDIRECT*i)));
                int64_t data_seq_num = third_block.last_level_block[l].last_updated;
                if (data_seq_num > seq_num) {
                  results.push_back({volume_offset, data_seq_num});
                }
              }
            }
          }
        }
      }
    }
  }

  return results;
}

void commit(long sequence_number, long volume_offset) {
    storage_mutex.lock();
  uncommitted_write* uw = uncommitted_writes[sequence_number];
  uncommitted_writes.erase(sequence_number);

  int64_t remainder = volume_offset % BLOCK_SIZE;
  volume_offset -= remainder;
  first_block.first_level_blocks[get_first_level(volume_offset)] = uw->new_metadata_offset[0];
  pending_blocks.erase(volume_offset);
  if (uw->new_metadata_offset[1] != 0 && uw->new_metadata_offset[0] != uw->new_metadata_offset[1]) {
    volume_offset += 4096;  
    first_block.first_level_blocks[get_first_level(volume_offset)] = uw->new_metadata_offset[1];
    pending_blocks.erase(volume_offset);
  }

  first_block.last_committed = sequence_number;

  // fsync twice to make sure that the actual data is persistent before modifying the metadata
  if (fdatasync(fd) == -1) {
      storage_mutex.unlock();
    perror("fsync");
    exit(1);
  }
  write_block(&first_block, 0);
  if (fdatasync(fd) == -1) {
      storage_mutex.unlock();
    perror("fsync");
    exit(1);
  }

  for (int64_t num : uw->to_free) {
    free_blocks.push(num);
  }
  storage_mutex.unlock();

  delete uw;
}

long get_sequence_number() {
  return first_block.last_committed;
}

std::string checksum() {
  char data[BLOCK_SIZE];
  int results_size = NUM_BLOCKS * MD5_DIGEST_LENGTH;
  unsigned char* results = new unsigned char[results_size];

  int used = 0;

  for (int i = 0; i < NUM_FIRST_LEVEL; ++i) {
    int64_t first_block_num = first_block.first_level_blocks[i];
    if (first_block_num != 0) {
      metadata_block first_block;
      read_block(&first_block, first_block_num);

      for (int j = 0; j < NUM_INDIRECT; ++j) {
        int64_t second_block_num = first_block.indirect_block[j];
        if (second_block_num != 0) {
          metadata_block second_block;
          read_block(&second_block, second_block_num);

          for (int k = 0; k < NUM_INDIRECT; ++k) {
            int64_t third_block_num = second_block.indirect_block[k];
            if (third_block_num != 0) {
              metadata_block third_block;
              read_block(&third_block, third_block_num);

              for (int l = 0; l < NUM_ENTRIES; ++l) {
                int64_t data_block_num = third_block.last_level_block[l].file_block_num;
                if (data_block_num != 0) {
                  read_block(data, data_block_num);
                  MD5((unsigned char*) data, BLOCK_SIZE, results + used);
                  used += MD5_DIGEST_LENGTH;
                }
              }
            }
          }
        }
      }
    }
  }

  unsigned char final_result[MD5_DIGEST_LENGTH];

  MD5(results, used, final_result);

  std::string to_return;
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
    ss << (int)final_result[i];
  }
  ss >> to_return;

  delete[] results;

  return to_return;
}


} // namespace Storage
