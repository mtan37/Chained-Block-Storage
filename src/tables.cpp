#include <queue>
#include <map>
#include <list>
#include <utility>
#include <exception>
#include <stdexcept>
#include <iostream>
#include "headers/tables.hpp"

namespace Tables {

    int currentSeq;
    int nextSeq;

    /**
    * Pending Queue
    */
    std::priority_queue<pendingQueueEntry> pendingQueue;

    void pushPendingQueue(pendingQueueEntry entry) {
       pendingQueue.push(entry);
    }

    pendingQueueEntry popPendingQueue() {
       pendingQueueEntry entry = pendingQueue.top();
       pendingQueue.pop();
       return entry;
    }

    /**
     * Checks the next seq # in the list
     * @return seq # that is at the top
     */
    int peekPendingQueue(){
        pendingQueueEntry entry = pendingQueue.top();
        return entry.seqNum;
    }

    int pendingQueueSize() {
       return pendingQueue.size();
    }

   /**
    * Sent List
    * Need to add and find items out of sequence, but want ordered
    * so we can iterate through on recovery
    */
//  typedef std::pair <int, sentListEntry> sentListPair;
    // <seq # (key), sentList entry (value)
    std::map<int, sentListItem> sentList;

    /**
     * adds entry to sentList at seqNum
     *
     * @param seqNum - key
     * @param entry  - sentListEntry to add to sent list at key
     */
    void pushSentList(sentListEntry entry) {
       //sentList.push(entry);
       auto ret = sentList.insert(entry);
       if (ret.second == false) {
           throw std::invalid_argument("Key already in list");
       }
    }

    /**
    * Remove single item from sentList, returns error if key not found
     *
    * @param seqNum
    * @return
    */
    sentListEntry popSentList(int seqNum) {
        sentListEntry entry;
        if (sentListSize()==0) throw std::length_error("Trying to get key from empty list");
        auto it = sentList.find(seqNum);
        if (it != sentList.end()){
            entry = *it;
            sentList.erase(it);
        } else {
           // throw error
           throw std::invalid_argument("Key not found in sentList");
        }
        return entry;
    }


    /**
     * returns a range of items >= startSeqNum, removes from sent list,
     * used for recovery.  If no startSeqNum, or = 0, returns and clears
     * entire sent list. Removes items as it adds them to the list
     *
     * @param startSeqNum sequence number at which to start returning values, defaults to 0
     *                     which returns entire list
     * @return list of sentListPairs starting at startSeqNum
     */
    std::list<sentListEntry> popSentListRange(int startSeqNum){
        std::list<sentListEntry> returnList;
        std::map<int, sentListItem>::iterator it;
        // Return if list is empty
        if (sentListSize()==0) throw std::length_error("Trying to get range from empty list");
        // Set iterator to correct location based on startSeqNum
        if (startSeqNum == 0) it = sentList.begin();
        else {
            it = sentList.find(startSeqNum);
            if (it == sentList.end()){
               throw std::invalid_argument("Key not found in sentList");
            }
        }
        while(it != sentList.end()){
            sentListEntry returnItem;
            returnItem = *it;
            returnList.push_back(returnItem);
            it = sentList.erase(it);
        }
        return returnList;
    }
   
    int sentListSize() {
       return sentList.size();
    }

    void printSentList() {
       for (auto it=sentList.begin(); it!=sentList.end(); ++it){
           std::cout << "    [key:file offsetA,B] = [" << it->first << "][" <<
           it->second.fileOffset[0] << ", " <<
           it->second.fileOffset[1] << "]" << std::endl;
       }
    }

    /***
     * Replay log
     *
     * Replay log is a little more involved.  We need to be able to check
     * for a full client ID, if it is present return sequence number, if not then
     * add to list and return 0.  So need to be able to find ip:pid:ts
     *
     * We also need to be able to find an item with client id, and remove all
     * items with same ip:port but previous timestamp to current client id. This
     * means that items need to be sorted or grouped by ip:port:ts, but map only
     * sorts by key.
     *
     * Finally we need to be able to run garbage collection,
     * looking over every item in list and removing it if time stamp is > X seconds old
     *
     * Multimap might work for sorting on multiple values in key, and then value is just
     * seq ID. SeqID is used both to let client know item was already played, and to aid
     * in recovery on mid failure. Having to find seq # as value would be problematic.
     * Might be worth just adding clientID to sent list.
     *
    */
//    std::priority_queue<replayLogEntry> replayLog;
    std::map<clientID, int> replayLog;

    /***
     * Adds item to replay log, unless it is already present, in which case it returns seq num
     * @param entry entry to add to list
     * @return 0 if added, seq number if already in list
     */
    int pushReplayLog(replayLogEntry entry) {
        auto ret = replayLog.insert(entry);
        if (ret.second == false) {
            return ret.first->second;
        }
        return 0;
    }

    replayLogEntry popReplayLog() {
//       replayLogEntry entry = replayLog.top();
//       replayLog.pop();
//       return entry;
    }

    int replayLogSize() {
       return replayLog.size();
    }

    void printReplayLog() {
        for (auto it=replayLog.begin(); it!=replayLog.end(); ++it){
            std::cout << "    [ClientID, seqID] = [" <<
                      it->first.ip << "::" << it->first.pid <<  "::" <<
                      it->first.rid  << ", " << it->second << "]" << std::endl;
        }
    }
   
};