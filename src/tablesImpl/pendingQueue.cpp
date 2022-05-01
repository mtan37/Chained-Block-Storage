#include <queue>

#include "tables.hpp"

namespace Tables {
   int currentSeq = 0;
   int nextSeq = 0;

   PendingQueue pendingQueue;

   void PendingQueue::pushEntry(pendingQueueEntry entry) {
      queue_mutex.lock();
      this->queue.push(entry);
      queue_mutex.unlock();
   }

   PendingQueue::pendingQueueEntry PendingQueue::popEntry() {
      queue_mutex.lock();
      pendingQueueEntry entry = this->queue.top();
      this->queue.pop();
      queue_mutex.unlock();
      return entry;
   }

   int PendingQueue::getQueueSize() {
      return this->queue.size();
   }
}