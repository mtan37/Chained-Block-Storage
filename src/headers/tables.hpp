#ifndef tables_hpp
#define tables_hpp

#include "server.grpc.pb.h"

#include <queue>
#include <string>
#include <map>
#include <utility>
#include <list>
#include <unordered_map>
#include <shared_mutex>

namespace Tables {

    extern long writeSeq; // What is the sequence number of the last written item
    extern long commitSeq; // what was the sequence number of the last committed write
    extern std::atomic<long> currentSeq; // What is the next available sequence number to assign to a new write

    class PendingQueue {
        public:
            struct pendingQueueEntry {
                long seqNum = -1;
                long volumeOffset = -1;
                //Not sure what type data needs to be yet
                std::string data = "";
                server::ClientRequestId reqId;

                bool operator<(const pendingQueueEntry& comp) const {
                    return seqNum > comp.seqNum;
                }
            };

            // Adds entry to queue
            void pushEntry(pendingQueueEntry entry);
            // removes and returns entry from priority queue
            pendingQueueEntry popEntry();
            pendingQueueEntry peekEntry();
            // returns size of priority queue
            int getQueueSize();

        private:
            std::priority_queue<pendingQueueEntry> queue;
            std::set<int> seqNumSet;// seq of seq number that is currently present in the queue
            std::mutex queue_mutex;
    };

    extern PendingQueue pendingQueue;

   /**
    * Sent List
    * And ordered map using clientId as key for fast retrival
    */
    class SentList {
        public:
            struct sentListEntry {
                server::ClientRequestId reqId;
                long volumeOffset = -1;
            };

            // Adds sentListEntry onto list. throws invalid_argument if key is already in list
            void pushEntry(int seqNum, sentListEntry entry);
            // Pops sentListEntry at clientId
            // throws invalid_argument if key is not in list
            sentListEntry popEntry(int seqNum);

            /**
             * returns a range of items >= startSeqNum,
             * used for recovery.  If no startSeqNum, or = 0, returns
             * entire sent list.
             *
             * @param startSeqNum sequence number at which to start 
             * returning values, defaults to 0 which returns entire list
             * 
             * @return list of sentListPairs starting at startSeqNum
             */
            std::map<int, sentListEntry> getSentListRange(int startSeqNum);
            // Returns size of sent list
            int getListSize();
            // Prints content of sent list
            void printSentList();
        private:
            std::map<int, sentListEntry> list;
            std::mutex list_mutex;

    };
    extern SentList sentList;

    // comparator used to compare different google timestamp
    struct googleTimestampComparator {
        bool operator() (
            const google::protobuf::Timestamp &t1, 
            const google::protobuf::Timestamp &t2) const {
            if (t1.seconds() < t2.seconds()) return true;
            else if (t1.seconds() == t2.seconds() && t1.nanos() < t2.nanos()) return true;
            return false;
        }
    };

    class ReplayLog {
        public:
            /**
             * @brief Add client entry to log if it 
             *      does not exist(and not already ack'ed)
             * 
             * @return 0 if the request is good and an entry is added
             *      -1 if the entry already exist
             *      -2 if the request is too old
             */
            int addToLog(server::ClientRequestId client_request_id);

            /**
             * @brief Remove an log entry(and entries with older id) when 
             *      ack is sent to client
             * 
             * @return 0 if the committed log is removed successfully
             *      -1 if the entry does not present in the log
             *      -2 if the entry is in the log, but not yet commited locally 
             */
            int ackLogEntry(server::ClientRequestId client_request_id);

            /**
             * @brief Used to mark an entry as commited(locally)
             * 
             * @return 0 if an uncommited log exist
             *      -1 if the log does not exist
             *      -2 if the log is already commited
             */
            int commitLogEntry(server::ClientRequestId client_request_id);

            /**
             * @brief Remove entires older than given age in seconds
             * 
             */
            void cleanOldLogEntry(time_t age);

            /**
             * @brief Print out the content of the replay log
             * 
             */
            void printRelayLogContent();

            /**
             * @brief initialize the replay log to the passed in UpdateReplayLogRequest
             * Note the current content will be lost if the log is not empty
             * 
             */
            void initRelayLogContent(server::UpdateReplayLogRequest content);

            /**
             * @brief get a copy of the replay log content in the passed in 
             * UpdateReplayLogRequest structure
             * 
             */
            void getRelayLogContent(server::UpdateReplayLogRequest &content);

        private:
            struct replayLogEntry {
                int test_val = 1;
                std::mutex client_entry_mutex;// mutext specific to modifying the client's entry
                std::map<google::protobuf::Timestamp,
                    bool, Tables::googleTimestampComparator> timestamp_list;
            };
            std::mutex new_entry_mutex;
            // mutex used so the edit of timestamp 
            // or addition of new entry does not happen at the same time as garbage collection
            mutable std::shared_mutex gc_entry_mutex;
            std::unordered_map<std::string, replayLogEntry *> client_list;
    };
    extern ReplayLog replayLog;
};
#endif