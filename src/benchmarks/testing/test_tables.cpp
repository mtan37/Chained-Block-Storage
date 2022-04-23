#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "../../headers/tables.hpp"

// g++ test_tables.cpp ../../tables.cpp -o test_tables
using namespace Tables;

const int NUM_ITEMS = 10;

/**
 * Expect pending queue to return items starting with lowest value
 */
void testPendingQueue(){
    for (int i = 0; i < NUM_ITEMS; i++) {
        struct pendingQueueEntry pendingEntry;
        pendingEntry.seqNum = rand() % NUM_ITEMS * 2 + 1;
        //std::cout << "Adding " << pendingEntry.seqNum << std::endl;
        pushPendingQueue(pendingEntry);
    }

    //std::cout << "pendingQueue Size = " << pendingQueueSize() << std::endl;
    assert(pendingQueueSize() == NUM_ITEMS);
    int current = -1, last;
    for (int i = 0; i < NUM_ITEMS; i++){
        last = current;
        struct pendingQueueEntry currentEntry = popPendingQueue();
        current = currentEntry.seqNum;
        //std::cout << "Entry " << i << " = " << current << std::endl;
        assert(current>=last);
    }
}

void testSentList(){
    int seqNum;
    sentListEntry entry;

    for (int i = 0; i < NUM_ITEMS; i++) {
        seqNum = rand() % NUM_ITEMS * 2 + 1;
        entry.fileOffset = i;
        std::cout << "Adding " << seqNum << std::endl;
        pushSentList(seqNum, entry);
    }

    for (auto it=sentList.begin(); it!=sentList.end(); ++it){
        std::cout << "[key:file offset] = [" << it->first << "][" << it->second.fileOffset << "]" << std::endl;
    }

    //std::cout << "pendingQueue Size = " << pendingQueueSize() << std::endl;
//    assert(pendingQueueSize() == NUM_ITEMS);
//    int current = -1, last;
//    for (int i = 0; i < NUM_ITEMS; i++){
//        last = current;
//        struct pendingQueueEntry currentEntry = popPendingQueue();
//        current = currentEntry.seqNum;
//        //std::cout << "Entry " << i << " = " << current << std::endl;
//        assert(current>=last);
//    }
}

void testReplayLog(){

}

// Basic unit testing on tables
int main(){
    srand (time(NULL));
    std::cout << "Testing pending queue...";
    // Test that items are removed sequentially and that size works
    testPendingQueue();
    std::cout << "passed" << std::endl << "Testing sent list ordering....";
    testSentList();
    std::cout << "passed" << std::endl;
}