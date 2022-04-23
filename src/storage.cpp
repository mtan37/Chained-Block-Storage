#include <algorithm>
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
#define NUM_BLOCKS VOLUME_SIZE/BLOCK_SIZE
#define NUM_ENTRIES BLOCK_SIZE / 16
#define NUM_INDIRECT BLOCK_SIZE / 8
#define NUM_FIRST_LEVEL NUM_BLOCKS / NUM_ENTRIES / NUM_INDIRECT / NUM_INDIRECT

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


namespace Storage {

void open_volume(std::string volume_name) {
  init_storage(volume_name.c_str());

  struct stat statbuf;
  if (fstat(fd, &statbuf) == -1) {
    perror("fstat");
    exit(1);
  }

  int64_t blocks_in_file = statbuf.st_size / BLOCK_SIZE;
  
  if (blocks_in_file == 0) {
    memset(&first_block, 0, sizeof(first_block));
    write_block(&first_block, 0);
  }
  else {
    read_block(&first_block, 0);
  }

  std::vector<int64_t> used_blocks;
  used_blocks.push_back(0);

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
  int block_num = 0;
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
  buf.resize(BLOCK_SIZE);
  write(buf.data(), volume_offset, file_offset);
}

void write(std::vector<char> buf, long volume_offset, long file_offset[2]) {
  buf.resize(BLOCK_SIZE);
  write(&buf[0], volume_offset, file_offset);
}

void write(const char* buf, long volume_offset, long file_offset[2]) {
  int64_t remainder = volume_offset % BLOCK_SIZE;

  //TODO: handle case when there are no free blocks

  if (remainder) {
    int64_t offset = volume_offset - remainder;
    char* buf1 = new char[BLOCK_SIZE];

    int64_t block_num = free_blocks.front();
    free_blocks.pop();
    read(buf1, offset);
    memcpy(buf1 + remainder, buf, BLOCK_SIZE - remainder);
    write_block(buf1, block_num);
    file_offset[0] = block_num;

    ++offset;
    block_num - free_blocks.front();
    free_blocks.pop();
    read(buf1, offset);
    memcpy(buf1, buf + BLOCK_SIZE - remainder, remainder);
    write_block(buf1, block_num);
    file_offset[1] = block_num;

    delete[] buf1;
  }
  else {
    int64_t block_num = free_blocks.front();
    free_blocks.pop();
    write_block(buf, block_num);
    file_offset[0] = block_num;
    file_offset[1] = -1;
  }
}

void read(std::string buf, long volume_offset) {
  read(buf.data(), volume_offset);
}

void read(std::vector<char> buf, long volume_offset) {
  read(&buf[0], volume_offset);
}

void read(char* buf, long volume_offset) {
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
  return;
}

void commit(long sequence_number, long file_offset[2], long volume_offset) {
}

} // namespace Storage
