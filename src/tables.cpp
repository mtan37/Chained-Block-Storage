#include <queue>
#include "tables.hpp"

namespace Tables {

   int currentSeq;
   int nextSeq;
    
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
   
   std::priority_queue<sentListEntry> sentList;
   
   void pushSentList(sentListEntry entry) {
       sentList.push(entry);
   }
   
   sentListEntry popSentList() {
       sentListEntry entry = sentList.top();
       sentList.pop();
       return entry;
   }
   
   int sentListSize() {
       return sentList.size();
   }
   
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