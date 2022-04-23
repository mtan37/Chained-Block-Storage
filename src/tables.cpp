#include <queue>
#include <map>
#include <utility>
#include <exception>
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
   
   int pendingQueueSize() {
       return pendingQueue.size();
   }

   /**
    * Sent List
    */
   // std::priority_queue<sentListEntry> sentList;
//   typedef std::pair <int, sentListEntry> sentListPair;
    std::map<int, sentListEntry> sentList;
   
   void pushSentList(int seqNum, sentListEntry entry) {
       //sentList.push(entry);
       sentList.insert(sentListPair(seqNum, entry));
   }
   
   sentListEntry popSentList(int seqNum) {
//       sentListEntry entry = sentList.top();
//       sentList.pop();
//       return entry;
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
   
   int sentListSize() {
       return sentList.size();
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