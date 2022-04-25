#ifndef tables_hpp
#define tables_hpp

//#include "server.grpc.pb.h"

#include <queue>
#include <string>
#include <map>
#include <utility>
#include <list>

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
        clientID reqId;

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

    /***
    * Replay log
    */

    // replay log data structures
    // <client id, seq #>
    typedef std::pair <clientID, int> replayLogEntry;
    extern std::map<clientID, int> replayLog;
    // Add to replay log and return -1, if clientID found, return SeqID
    extern int pushReplayLog(replayLogEntry entry);
    // remove an log entry(and entries with older id) when ack is sent to client
    // return -1 if the entry does not present in the log 
    int ackLogEntry(clientID clientRequestId);
    // Return size of replay log
    extern int replayLogSize();
    // Print contents of replay log
    void printReplayLog();
    // remove entires older than given age in seconds - used for garbage collection
    void cleanOldLogEntry(time_t age);
       
};
#endif