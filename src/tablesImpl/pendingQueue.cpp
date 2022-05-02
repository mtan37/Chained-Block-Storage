#include <queue>

#include "tables.hpp"

namespace Tables {
   int writeSeq = 0;
   int commitSeq = 0;

   PendingQueue pendingQueue;

   void PendingQueue::pushEntry(pendingQueueEntry entry) {
      queue_mutex.lock();
      std::pair<std::set<int>::iterator,bool> ret = seqNumSet.insert(entry.seqNum);
      if (!ret.second) {
         queue_mutex.unlock();
         std::string err_msg = "sequence number "
            + std::to_string(entry.seqNum) + " already exist in pending queue";
         throw std::invalid_argument(err_msg);
      }
      this->queue.push(entry);
      queue_mutex.unlock();
   }

   PendingQueue::pendingQueueEntry PendingQueue::popEntry() {
      queue_mutex.lock();
      pendingQueueEntry entry = this->queue.top();
      seqNumSet.erase(entry.seqNum);
      this->queue.pop();
      queue_mutex.unlock();
      return entry;
   }

   int PendingQueue::getQueueSize() {
      return this->queue.size();
   }
}