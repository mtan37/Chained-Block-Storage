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
        long double timestamp = 0;
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

    /***
    * Replay log
    */
    struct replayLogEntry {
       int seqNum;
       clientID reqID;

       bool operator<(const replayLogEntry& comp) const {
            return seqNum < comp.seqNum;
       }
    };

    extern std::priority_queue<replayLogEntry> replayLog;

    extern void pushReplayLog(replayLogEntry entry);
    extern replayLogEntry popReplayLog();
    extern int replayLogSize();
   
};
#endif