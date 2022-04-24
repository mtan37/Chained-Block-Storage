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
    std::map<int, sentListEntry> sentList;

    /**
     * adds entry to sentList at seqNum
     *
     * @param seqNum - key
     * @param entry  - sentListEntry to add to sent list at key
     */
    void pushSentList(int seqNum, sentListEntry entry) {
       //sentList.push(entry);
       auto ret = sentList.insert(sentListPair(seqNum, entry));
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
        auto it = sentList.find(seqNum);
        if (it != sentList.end()){
            entry = it->second;
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
    std::list<sentListPair> popSentListRange(int startSeqNum){
        std::list<sentListPair> returnList;
        std::map<int, sentListEntry>::iterator it;
        if (sentListSize()==0) return returnList;
        if (startSeqNum == 0) it = sentList.begin();
        else {
            it = sentList.find(startSeqNum);
            if (it == sentList.end()){
               throw std::invalid_argument("Key not found in sentList");
            }
        }
        while(it != sentList.end()){
            sentListPair returnItem;
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
           std::cout << "[key:file offsetA,B] = [" << it->first << "][" <<
           it->second.fileOffset[0] << ", " <<
           it->second.fileOffset[1] << "]" << std::endl;
       }
    }

    /***
    * Replay log
    */
    std::priority_queue<replayLogEntry> replayLog;

    void pushReplayLog(replayLogEntry entry) {
       replayLog.push(entry);
    }

    replayLogEntry popReplayLog() {
       replayLogEntry entry = replayLog.top();
       replayLog.pop();
       return entry;
    }

    int replayLogSize() {
       return replayLog.size();
    }
   
};