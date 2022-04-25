#ifndef tables_hpp
#define tables_hpp

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
        clientID reqID;

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
    // Data structures
    struct sentListItem {
       //int seqNum; // Seq num is the key itself
       int volumeOffset = -1;
       int fileOffset[2] = {-1,-1};
    };
    extern std::map<int, sentListItem> sentList;
    typedef std::pair <int, sentListItem> sentListEntry; // this is how the map above stores this data
    // functions
    extern void pushSentList(sentListEntry entry);
    extern sentListEntry popSentList(int seqNum);
    extern std::list<sentListEntry> popSentListRange(int startSeqNum = 0);
    extern int sentListSize();
    void printSentList();

    /***
    * Replay log
    */
//    struct replayLogEntry {
//       int seqNum;
//       clientID reqID;
//
//       bool operator<(const replayLogEntry& comp) const {
//            return reqID < comp.reqID;
//       }
//    };
    typedef std::pair <clientID, int> replayLogEntry;
    extern std::map<clientID, int> replayLog;

    extern int pushReplayLog(replayLogEntry entry);
    extern replayLogEntry popReplayLog();
    extern int replayLogSize();
    void printReplayLog();
   
};
#endif