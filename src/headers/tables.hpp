#ifndef tables_hpp
#define tables_hpp

#include "server.grpc.pb.h"

#include <queue>
#include <string>
#include <map>
#include <utility>
#include <list>
#include <unordered_map>

namespace Tables {

    extern int currentSeq;
    extern int nextSeq;

    /**
    * Pending Queue
    */
    // Entry
    struct pendingQueueEntry {
        int seqNum = -1;
        int volumeOffset = -1;
        //Not sure what type data needs to be yet
        std::string data = "";
        //How do we want to store client IDs?
        server::ClientRequestId reqId;

        bool operator<(const pendingQueueEntry& comp) const {
            return seqNum > comp.seqNum;
        }
    };
    // List
    extern std::priority_queue<pendingQueueEntry> pendingQueue;
    // methods
    extern void pushPendingQueue(pendingQueueEntry entry);
    extern pendingQueueEntry popPendingQueue();
    extern int pendingQueueSize();

    /**
    * Sent List
    */
    // Entry
    struct sentListEntry {
       //int seqNum; // Seq num is the key itself
       int volumeOffset = -1;
       int fileOffset[2] = {-1,-1};
    };

    // List and key value pair
    //extern std::priority_queue<sentListEntry> sentList;
    extern std::map<int, sentListEntry> sentList;
    typedef std::pair <int, sentListEntry> sentListPair;
    // functions
    extern void pushSentList(int seqNum, sentListEntry entry);
    extern sentListEntry popSentList(int seqNum);
    extern std::list<sentListPair> popSentListRange(int startSeqNum = 0);
    extern int sentListSize();
    void printSentList();

    struct googleTimestampComparator {
        bool operator()(
            const google::protobuf::Timestamp &t1, 
            const google::protobuf::Timestamp &t2) {
            if (t1.seconds() < t2.seconds()) return true;
            else if (t1.seconds() == t2.seconds() && t1.nanos() < t2.nanos()) return true;
            return false;
        }
    };

    class ReplayLog {
        public:
            // add client entry to log if it does not exist(and not already ack'ed)
            // return -1 if the entry exist
            int addToLog(server::ClientRequestId client_request_id);
            // remove an log entry(and entries with older id) when ack is sent to client
            // return -1 if the entry does not present in the log 
            int ackLogEntry(server::ClientRequestId client_request_id);
            // remove entires older than given age in seconds
            void cleanOldLogEntry(time_t age);
            void printRelayLogContent();
        private:
            struct replayLogEntry {
                int test_val = 1;
                std::mutex client_entry_mutex;// mutext specific to modifying the client's entry
                std::set<google::protobuf::Timestamp, Tables::googleTimestampComparator> timestamp_list;
            };
            std::mutex new_entry_mutex;
            std::unordered_map<std::string, replayLogEntry *> client_list;
    };

    extern ReplayLog replayLog;
};
#endif