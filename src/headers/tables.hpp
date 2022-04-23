#ifndef tables_hpp
#define tables_hpp

#include <queue>
#include <string>
#include <map>
#include <utility>

namespace Tables {

    extern int currentSeq;
    extern int nextSeq;

    //Replace later with data that directly references IP/pid/timestamp byte values
    struct clientID {
        std::string ip;
        int pid;
        long double timestamp;
    };

    struct pendingQueueEntry {
        int seqNum;
        int volumeOffset;
        //Not sure what type data needs to be yet
        std::string data;
        //How do we want to store client IDs?
        clientID reqID;

        bool operator<(const pendingQueueEntry& comp) const {
            return seqNum > comp.seqNum;
        }
    };

    /**
    * Pending Queue
    */
    extern std::priority_queue<pendingQueueEntry> pendingQueue;


    extern void pushPendingQueue(pendingQueueEntry entry);
    extern pendingQueueEntry popPendingQueue();
    extern int pendingQueueSize();

    /**
    * Sent List
    */
    struct sentListEntry {
       //int seqNum; // Seq num is the key itself
       int volumeOffset;
       int fileOffset;

//       bool operator<(const sentListEntry& comp) const {
//           return seqNum < comp.seqNum;
//       }
    };

    //extern std::priority_queue<sentListEntry> sentList;
    extern std::map<int, sentListEntry> sentList;
    typedef std::pair <int, sentListEntry> sentListPair;
    extern void pushSentList(int seqNum, sentListEntry entry);
    extern sentListEntry popSentList(int seqNum);
    extern int sentListSize();

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