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

    //Replace later with data that directly references IP/pid/timestamp byte values
    struct clientID {
        std::string ip = "";
        int pid = -1;
        double rid = 0;

        bool operator<(const clientID& comp) const {
            if (ip == comp.ip){
                if (pid == comp.pid) {
                    return rid < comp.rid;
                } else return pid < comp.pid;
            } else return ip < comp.ip;
        }
    };
    
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
    extern std::priority_queue<pendingQueueEntry> pendingQueue;

    // methods
    // Adds entry to queue
    extern void pushPendingQueue(pendingQueueEntry entry);
    // removes and returns entry from priority queue
    extern pendingQueueEntry popPendingQueue();
    // returns size of priority queue
    extern int pendingQueueSize();

    /**
    * Sent List
    */

    // Data structures
    struct sentListItem {
       //int seqNum; // Seq num is the key itself
       int volumeOffset = -1;
       int fileOffset[2] = {-1,-1};
    };
    extern std::map<int, sentListItem> sentList;
    typedef std::pair <int, sentListItem> sentListEntry; // this is how the map above stores this data
    
    // Methods
    // Adds sentListEntry onto list. throws invalid_argument if key is already in list
    extern void pushSentList(sentListEntry entry);
    // Pops sentListEntry at seqNum
    extern sentListEntry popSentList(int seqNum);
    // pops and returns a list of sentListEntry's from key-EOF)  that can be used to 
    // aid recovery on failur. Throws invalid_argument if startSeqNum
    // is not in list, throws length_error if list is empty
    extern std::list<sentListEntry> popSentListRange(int startSeqNum = 0);
    // Returns size of sent list
    extern int sentListSize();
    // Prints content of sent list
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
             * @return 0 if an uncommited log exist and is committed
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

        private:
            struct replayLogEntry {
                int test_val = 1;
                std::mutex client_entry_mutex;// mutext specific to modifying the client's entry
                std::map<google::protobuf::Timestamp, bool, Tables::googleTimestampComparator> timestamp_list;
            };
            std::mutex new_entry_mutex;
            std::unordered_map<std::string, replayLogEntry *> client_list;
    };

    extern ReplayLog replayLog;
};
#endif