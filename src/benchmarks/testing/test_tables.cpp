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
    int seqNum;
    sentListEntry entry;

    // adding to list, should never cause an issue
    int foundSeq[NUM_ITEMS];
    for (int i = 0; i < NUM_ITEMS; i++) {
        seqNum = rand() % NUM_ITEMS * 2 + 1;
        entry.fileOffset[0] = i;
        try{
            pushSentList(seqNum, entry);
//            std::cout << "Adding " << seqNum << ", " << entry.fileOffset[0] << std::endl;
            foundSeq[i] = seqNum;
        } catch (std::invalid_argument e) {
            //std::cout << e.what() << std::endl;
            i--;
        }
    }

    // Test that correct number of items in list
//    std::cout << "sentList Size = " << sentListSize() << std::endl;
    assert(sentListSize() == NUM_ITEMS);
//    printSentList();

    // Test removing invalid entry
    try {
        popSentList(999);
        assert(0);
    } catch (std::invalid_argument e) {
        assert(1);
    }

    // Test that all items are found on list, and can be removed without issue
    for (int i = 0; i < NUM_ITEMS; i++){
        try{
            popSentList(foundSeq[i]);
        } catch (std::invalid_argument e) {
//            std::cout << "Entered key not found" << std::endl;
            assert(0);
        }
    }
    // Test that size after removal is 0
    assert(sentListSize() == 0);

}

void testSentListRange(){
    // Add items to list
    sentListEntry entries[5];
    int seqNums[5] {4,9, 3, 7, 2};
    entries[0].fileOffset[0] = 12;
    entries[1].fileOffset[0] = 6;
    entries[2].fileOffset[0] = 5;
    entries[3].fileOffset[0] = 419;
    entries[4].fileOffset[0] = 22;
    for (int i = 0; i < 5; i++){
        pushSentList(seqNums[i], entries[i]);
    }

//    printSentList();

    // Test removal of partial range (4-9)
//    std::cout << "Checking range starting at 4" << std::endl;
    std::list<sentListPair> partialList = popSentListRange(4);
    int i = 0;
    for (sentListPair item: partialList){
//        std::cout << "Found [" << item.first << ", " << item.second.fileOffset[0] << "]" << std::endl;
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


    // Test removel of all items in list
//    std::cout << "Checking range starting at 0" << std::endl;
    std::list<sentListPair> finalList = popSentListRange();
    i=0;
    for (sentListPair item: finalList){
//        std::cout << "Found [" << item.first << ", " << item.second.fileOffset[0] << "]" << std::endl;
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
    std::cout << "passed" << std::endl << "Testing sent list range....";
    testSentListRange();
    std::cout << "passed" << std::endl;
}