#include <grpc++/grpc++.h>
#include <thread>
#include "server.h"
#include "tables.hpp"

int popInvalidEntry() {
    // add an invalid entry, and make sure it returns the exception
    try {
        Tables::sentList.popEntry(233);
        return -1;
    } catch (std::invalid_argument e) {
        return 0;
    }
}

int pushRepeatEntry() {
    // add an invalid entry, and make sure it returns the exception
    try {
        Tables::SentList::sentListEntry sent_entry;
        Tables::sentList.pushEntry(122, sent_entry);
        Tables::sentList.printSentList();
        Tables::sentList.pushEntry(122, sent_entry);
        return -1;
    } catch (std::invalid_argument e) {
        Tables::sentList.popEntry(122);
        return 0;
    }
}

/**
 * @brief Add, then remove a list of sent list entry 
 * for the seq number at the given range
 * 
 */
void wordloadThread(int seq_start, int seq_end) {
    Tables::SentList::sentListEntry sent_entry;

    // add the entries
    for (int i = seq_start; i <= seq_end; i++) {
        Tables::sentList.pushEntry(i, sent_entry);
    }
    // remove the entires
    for (int i = seq_start; i <= seq_end; i++) {
        Tables::sentList.popEntry(i);
    }
}

void workLoadTest() {
    int x = 1;
    int y = 3;

    // start threads simulating addition of a list of entires, then removal
    std::thread workload1 (wordloadThread, 0, 500000);
    std::thread workload2 (wordloadThread, 500001, 1000000);
    std::thread workload3 (wordloadThread, 1000001, 1500000);

    workload1.join();
    workload2.join();
    workload3.join();
}

int main(int argc, char *argv[]) {

    if (popInvalidEntry() < 0) {
        std::cout << "popInvalidEntry failed" << std::endl;
    } else {
        std::cout << "popInvalidEntry passed" << std::endl;
    }

    if (pushRepeatEntry() < 0) {
        std::cout << "pushRepeatEntry failed" << std::endl;
    } else {
        std::cout << "pushRepeatEntry passed" << std::endl;
    }

    // workload simulation test
    workLoadTest();
    std::cout << "wordload simulation passed" << std::endl;
}