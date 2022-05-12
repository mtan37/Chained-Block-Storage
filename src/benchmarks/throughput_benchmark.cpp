#include <iostream>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "client_api.hpp"
#include "constants.hpp"

using namespace std;

void usage(char *name) {
  printf("Usage: %s <write iteration> -is_unaligned\n", name);
  printf("  write iteration     number of iterations for the write\n");
  printf("  is_unaligned_write     if flag is present, write is not aligned. \n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    int write_count = atoi(argv[1]);
    bool is_aligned = true;
    if (argc > 2 && std::string(argv[2]).compare("is_unaligned") == 0) {
        cout << "doing unaligned write" << endl;
        is_aligned = false;
    }
    
    pid_t pid = getpid();
    cout << "Howdy~ I am process " << std::to_string(pid) << ". ";
    cout << "I do " << write_count << "";
    if (is_aligned) cout << " aligned write~" << endl;
    else cout << " unaligned write~" << endl;
    Client c = Client("localhost", "localhost");
    int init_offset = Constants::BLOCK_SIZE * (rand() % 100);
    if (!is_aligned) init_offset = Constants::BLOCK_SIZE - (rand() % 4000 + 1);

    const long MAX_OFFSET = init_offset  + 1024 * 1024 *  (long)Constants::BLOCK_SIZE;

    // write the same data to increase speed
    Client::DataBlock data;
    std::string write_content = "yesyes";
    memcpy(data.buff, write_content.c_str(), Constants::BLOCK_SIZE);
    google::protobuf::Timestamp tm;
    for (int i = 0; i < write_count; i ++) {
        long offset = (init_offset + i * Constants::BLOCK_SIZE ) % MAX_OFFSET;
        c.write(data, tm, offset);
    }

}