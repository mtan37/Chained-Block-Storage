#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "../../headers/tables.hpp"
#include "../../headers/helper.h"


// g++ test_tables.cpp ../../tables.cpp ../../helper.cpp -o test_tables
using namespace Tables;

const int NUM_ITEMS = 6;

/**
 * Expect pending queue to return items starting with lowest value
 */
void testPendingQueue(){
    std::list<int> added;
    for (int i = 0; i < NUM_ITEMS; i++) {
        struct pendingQueueEntry pendingEntry;
        pendingEntry.seqNum = rand() % NUM_ITEMS * 2 + 1;
        added.push_back(pendingEntry.seqNum);
        //std::cout << "Adding " << pendingEntry.seqNum << std::endl;
        pushPendingQueue(pendingEntry);
    }
    added.sort();

    //std::cout << "pendingQueue Size = " << pendingQueueSize() << std::endl;
    assert(pendingQueueSize() == NUM_ITEMS);
    int current = -1, last;
    for (int i = 0; i < NUM_ITEMS; i++){
        last = current;
        struct pendingQueueEntry currentEntry = popPendingQueue();
        current = currentEntry.seqNum;

        //std::cout << "Entry " << i << " = " << current << std::endl;
        assert(current>=last);
        assert(added.front() == current);
        added.pop_front();
    }


}

void testSentList(){
    sentListEntry entry;

    // adding to list, should never cause an issue
    int foundSeq[NUM_ITEMS];
    for (int i = 0; i < NUM_ITEMS; i++) {
        entry.first = rand() % NUM_ITEMS * 2 + 1;
        entry.second.fileOffset[0] = i;
        try{
            pushSentList(entry);
            std::cout << "    Adding " << entry.first << ", " << entry.second.fileOffset[0] << std::endl;
            foundSeq[i] = entry.first;
        } catch (std::invalid_argument e) {
//            std::cout << e.what() << std::endl;
            i--;
        }
    }

    // Test that correct number of items in list
    std::cout << "    sentList Size = " << sentListSize() << std::endl;
    assert(sentListSize() == NUM_ITEMS);
    printSentList();

    // Test removing invalid entry
    try {
        popSentList(999);
        assert(0);
    } catch (std::invalid_argument e) {
        std::cout << "    Caught error removing invalid entry" << std::endl;
        assert(1);
    }

    // Test that all items are found on list, and can be removed without issue
    for (int i = 0; i < NUM_ITEMS; i++){
        try{
            popSentList(foundSeq[i]);
        } catch (std::invalid_argument e) {
            std::cout << e.what() << std::endl;
            assert(0);
        }
    }
    // Test that size after removal is 0
    assert(sentListSize() == 0);

    // Test removing entry from empty list
    try {
        popSentList(999);
        assert(0);
    } catch (std::length_error e) {
        std::cout << "    Caught error removing entry from empty list" << std::endl;
        assert(1);
    }

}

void testSentListRange(){
    // Add items to list
    sentListEntry entries[5];
    int seqNums[5] {4,9, 3, 7, 2};
    entries[0].second.fileOffset[0] = 12;
    entries[1].second.fileOffset[0] = 6;
    entries[2].second.fileOffset[0] = 5;
    entries[3].second.fileOffset[0] = 419;
    entries[4].second.fileOffset[0] = 22;
    for (int i = 0; i < 5; i++){
        entries[i].first = seqNums[i];
        pushSentList(entries[i]);
    }

    printSentList();

    // test removal of items with invalid key > 0
    try{
        std::list<sentListEntry> partialList = popSentListRange(25);
        std::cout << "    Unexpectedly got entries from invalid key" << std::endl;
        assert(0);
    } catch (std::invalid_argument e) {
        std::cout << "    Caught error trying to remove range with invalid key" << std::endl;
        assert(1);
    }

    // Test removal of partial range (4-9)
    std::cout << "    Checking range starting at 4" << std::endl;
    std::list<sentListEntry> partialList = popSentListRange(4);
    int i = 0;
    for (sentListEntry item: partialList){
        std::cout << "        Found [" << item.first << ", " << item.second.fileOffset[0] << "]" << std::endl;
        switch (i) {
            case 0:
                assert(item.first == 4);
                break;
            case 1:
                assert(item.first == 7);
                break;
            case 2:
                assert(item.first == 9);
                break;
        }
        i++;
    }
    assert(partialList.size() == 3);


    // Test removal of all items in list
    std::cout << "    Checking range starting at 0" << std::endl;
    std::list<sentListEntry> finalList = popSentListRange();
    i=0;
    for (sentListEntry item: finalList){
        std::cout << "        Found [" << item.first << ", " << item.second.fileOffset[0] << "]" << std::endl;
        switch (i) {
            case 0:
                assert(item.first == 2);
                break;
            case 1:
                assert(item.first == 3);
                break;
        }
        i++;
    }
    assert(finalList.size() == 2);

    // Test trying to remove items from empty list
    try{
        std::list<sentListEntry> partialList = popSentListRange(4);
        std::cout << "    Unexpectedly found entry in empty list" << std::endl;
        assert(0);
    } catch (std::length_error e) {
        std::cout << "    Caught error trying to remove range from empty list" << std::endl;
        assert(1);
    }

}
void testReplayLog(){
    std::string ip_base = "192.168.0.";
    struct timespec ts;
    for (int i = 0; i  < NUM_ITEMS*2; i++){
        clientID cid;
        replayLogEntry entry;

        cid.ip = ip_base + std::to_string(i % 3 + 1);
        cid.pid = i % 2 + 1;
        cid.rid = get_time_ns(&ts);
        entry.first = cid;
        entry.second = i;
        pushReplayLog(entry);
    }

    printReplayLog();
}

// Basic unit testing on tables
int main(){
    srand (time(NULL));
    std::cout << "Testing pending queue..." << std::endl;
    // Test that items are removed sequentially and that size works
    testPendingQueue();
    std::cout << "passed" << std::endl << "Testing sent list ordering...." << std::endl;
    testSentList();
    std::cout << "passed" << std::endl << "Testing sent list range...." << std::endl;
    testSentListRange();
    std::cout << "passed" << std::endl << "Testing replay log...." << std::endl;
    testReplayLog();
    std::cout << "passed" << std::endl;
}