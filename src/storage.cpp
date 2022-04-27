#include <algorithm>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

#include <cmath>
#include <cstring>

#include <fcntl.h>
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

static std::queue<int64_t> free_blocks;

static int fd;

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
  fd = open(filename, O_RDWR | O_CREAT | O_SYNC, S_IRUSR | S_IWUSR);
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
    off_t res = lseek(fd, 0, SEEK_END);
    if (res == -1) {
      perror("lseek");
      exit(1);
    }
    return (res+1) / BLOCK_SIZE;
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
  int64_t blocks_in_file = (res + 1) / BLOCK_SIZE;
  
  if (blocks_in_file == 0) {
    memset(&first_block, 0, sizeof(first_block));
    write_block(&first_block, 0);
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

  while (block_num < blocks_in_file) {
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

void write(std::string buf, long volume_offset, long file_offset[2]) {
  write(buf.data(), volume_offset, file_offset);
}

void write(std::vector<char> buf, long volume_offset, long file_offset[2]) {
  write(&buf[0], volume_offset, file_offset);
}

void write(const char* buf, long volume_offset, long file_offset[2]) {
  int64_t remainder = volume_offset % BLOCK_SIZE;

  if (remainder) {
    int64_t offset = volume_offset - remainder;
    char* buf1 = new char[BLOCK_SIZE];

    int64_t block_num = get_free_block_num();
    read(buf1, offset);
    memcpy(buf1 + remainder, buf, BLOCK_SIZE - remainder);
    write_block(buf1, block_num);
    file_offset[0] = block_num;

    offset += BLOCK_SIZE;
    block_num = get_free_block_num();
    read(buf1, offset);
    memcpy(buf1, buf + BLOCK_SIZE - remainder, remainder);
    write_block(buf1, block_num);
    file_offset[1] = block_num;

    delete[] buf1;
  }
  else {
    int64_t block_num = get_free_block_num();
    write_block(buf, block_num);
    file_offset[0] = block_num;
    file_offset[1] = -1;
  }
}

void read(std::string buf, long volume_offset) {
  buf.resize(BLOCK_SIZE);
  read(buf.data(), volume_offset);
}

void read(std::vector<char> buf, long volume_offset) {
  buf.resize(BLOCK_SIZE);
  read(&buf[0], volume_offset);
}

void read(char* buf, long volume_offset) {
  int64_t remainder = volume_offset % BLOCK_SIZE;

  if (remainder) {
    int64_t offset = volume_offset - remainder;
    char* buf1 = new char[BLOCK_SIZE];

    read_aligned(buf, offset);

    offset += BLOCK_SIZE;
    read_aligned(buf1, offset);
    memcpy(buf, buf1 + BLOCK_SIZE - remainder, remainder);

    delete[] buf1;
  }
  else {
    read_aligned(buf, volume_offset);
  }
}

void commit(long sequence_number, long file_offset[2], long volume_offset) {
  int64_t remainder = volume_offset % BLOCK_SIZE;

  std::vector<int64_t> to_free;

  int num_file_blocks = 1;
  if (remainder) {
    num_file_blocks = 2;
  }

  metadata_block block[2][3];
  int64_t old_block_nums[][3] = {{0,0,0},{0,0,0}};
  int64_t offsets[][4] = {{0,0,0,0},{0,0,0,0}};
  int64_t v_offsets[] = {volume_offset - remainder, 
                         volume_offset - remainder + BLOCK_SIZE};
  for (int i = 0; i < num_file_blocks; ++i) {
    offsets[i][0] = get_first_level(v_offsets[i]);
    offsets[i][1] = get_second_level(v_offsets[i]);
    offsets[i][2] = get_third_level(v_offsets[i]);
    offsets[i][3] = get_last_level(v_offsets[i]);
    old_block_nums[i][0] = first_block.first_level_blocks[offsets[i][0]];
    if (old_block_nums[i][0] != 0) {
      read_block(&block[i][0], old_block_nums[i][0]);
      old_block_nums[i][1] = block[i][0].indirect_block[offsets[i][1]];
      if (old_block_nums[i][1] != 0) {
        read_block(&block[i][1], old_block_nums[i][1]);
        old_block_nums[i][2] = block[i][1].indirect_block[offsets[i][2]];
        if (old_block_nums[i][2] != 0) {
          read_block(&block[i][2], old_block_nums[i][2]);
        }
      }
    }
  }

  int64_t new_block_nums[2][3];
  for (int i = 0; i < num_file_blocks; ++i) {
    if (old_block_nums[i][2] == 0) {
      memset(&block[i][2], 0, BLOCK_SIZE);
    }
  }
  if (old_block_nums[0][2] != 0) {
    to_free.push_back(block[0][2].last_level_block[offsets[0][2]].file_block_num);
  }
  if (offsets[0][2] == offsets[1][2]) { 
    // the two blocks have the same metadata_blocks up to the third level
    new_block_nums[0][2] = new_block_nums[1][2] = get_free_block_num();
  }
  else {
    for (int i = 0; i < num_file_blocks; ++i) {
      new_block_nums[i][2] = get_free_block_num();
    }
    if (old_block_nums[1][2] != 0) {
      to_free.push_back(block[1][2].last_level_block[offsets[1][2]].file_block_num);
    }
  }
  for (int i = 0; i < num_file_blocks; ++i) {
    block[i][2].last_level_block[offsets[i][3]].file_block_num = file_offset[i];
    block[i][2].last_level_block[offsets[i][3]].last_updated = sequence_number;
    if (offsets[0][2] == offsets[1][2]) { // only write once if the same
      if (remainder) {
        block[i][2].last_level_block[offsets[i+1][3]].file_block_num = file_offset[i+1];
        block[i][2].last_level_block[offsets[i+1][3]].last_updated = sequence_number;
      }
      write_block(&block[i][2], new_block_nums[i][2]);
      break;
    }
    write_block(&block[i][2], new_block_nums[i][2]);
  }
  
  for (int i = 1; i >= 0; --i) {
    for (int j = 0; j < num_file_blocks; ++j) {
      if (old_block_nums[j][i] == 0) {
        memset(&block[j][i], 0, BLOCK_SIZE);
      }
    }
    if (offsets[0][i] == offsets[1][i]) {
      new_block_nums[0][i] = new_block_nums[1][i] = get_free_block_num();
    }
    else {
      for (int j = 0; j < num_file_blocks; ++j) {
        new_block_nums[j][i] = get_free_block_num();
      }
    }
    if (old_block_nums[0][i] != 0) {
      to_free.push_back(block[0][i].indirect_block[offsets[0][i]]);
    }
    if (old_block_nums[1][i] != 0 && old_block_nums[1][i] != old_block_nums[0][i]) {
        to_free.push_back(block[1][i].indirect_block[offsets[1][i]]);
    }
    for (int j = 0; j < num_file_blocks; ++j) {
      block[j][i].indirect_block[offsets[j][i]] = new_block_nums[j][i+1];
      if (offsets[1][i] == offsets[1][i]) {
        if (remainder) {
          block[j][i].indirect_block[offsets[j+1][i]] = new_block_nums[j+1][i+1];
        }
        write_block(&block[j][i], new_block_nums[j][i]);
        break;
      }
      write_block(&block[j][i], new_block_nums[j][i]);
    }
  }

  for (int i = 0; i < num_file_blocks; ++i) {
    first_block.first_level_blocks[offsets[i][0]] = new_block_nums[i][0];
  }

  first_block.last_committed = sequence_number;
  write_block(&first_block, 0);

  for (int64_t num : to_free) {
    free_blocks.push(num);
  }
}

long get_sequence_number() {
  return first_block.last_committed;
}



} // namespace Storage
