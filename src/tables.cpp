#include <queue>
#include <map>
#include <list>
#include <utility>
#include <exception>
#include <stdexcept>
#include <iostream>
#include "headers/tables.hpp"

namespace Tables {

    int currentSeq;
    int nextSeq;

    /**
    * Pending Queue
    */
   
    std::priority_queue<pendingQueueEntry> pendingQueue;

    void pushPendingQueue(pendingQueueEntry entry) {
       pendingQueue.push(entry);
    }

    pendingQueueEntry popPendingQueue() {
       pendingQueueEntry entry = pendingQueue.top();
       pendingQueue.pop();
       return entry;
    }

    /**
     * Checks the next seq # in the list
     * @return seq # that is at the top
     */
    int peekPendingQueue(){
        pendingQueueEntry entry = pendingQueue.top();
        return entry.seqNum;
    }

    int pendingQueueSize() {
       return pendingQueue.size();
    }

   /**
    * Sent List
    * Need to add and find items out of sequence, but want ordered
    * so we can iterate through on recovery
    */
//  typedef std::pair <int, sentListEntry> sentListPair;
    // <seq # (key), sentList entry (value)
    std::map<int, sentListItem> sentList;

    /**
     * adds entry to sentList at seqNum
     *
     * @param seqNum - key
     * @param entry  - sentListEntry to add to sent list at key
     */
    void pushSentList(sentListEntry entry) {
       //sentList.push(entry);
       auto ret = sentList.insert(entry);
       if (ret.second == false) {
           throw std::invalid_argument("Key already in list");
       }
    }

    /**
    * Remove single item from sentList, returns error if key not found
     *
    * @param seqNum
    * @return
    */
    sentListEntry popSentList(int seqNum) {
        sentListEntry entry;
        if (sentListSize()==0) throw std::length_error("Trying to get key from empty list");
        auto it = sentList.find(seqNum);
        if (it != sentList.end()){
            entry = *it;
            sentList.erase(it);
        } else {
           // throw error
           throw std::invalid_argument("Key not found in sentList");
        }
        return entry;
    }


    /**
     * returns a range of items >= startSeqNum, removes from sent list,
     * used for recovery.  If no startSeqNum, or = 0, returns and clears
     * entire sent list. Removes items as it adds them to the list
     *
     * @param startSeqNum sequence number at which to start returning values, defaults to 0
     *                     which returns entire list
     * @return list of sentListPairs starting at startSeqNum
     */
    std::list<sentListEntry> popSentListRange(int startSeqNum){
        std::list<sentListEntry> returnList;
        std::map<int, sentListItem>::iterator it;
        // Return if list is empty
        if (sentListSize()==0) throw std::length_error("Trying to get range from empty list");
        // Set iterator to correct location based on startSeqNum
        if (startSeqNum == 0) it = sentList.begin();
        else {
            it = sentList.find(startSeqNum);
            if (it == sentList.end()){
               throw std::invalid_argument("Key not found in sentList");
            }
        }
        while(it != sentList.end()){
            sentListEntry returnItem;
            returnItem = *it;
            returnList.push_back(returnItem);
            it = sentList.erase(it);
        }
        return returnList;
    }
   
    int sentListSize() {
       return sentList.size();
    }

    void printSentList() {
       for (auto it=sentList.begin(); it!=sentList.end(); ++it){
           std::cout << "    [key:file offsetA,B] = [" << it->first << "][" <<
           it->second.fileOffset[0] << ", " <<
           it->second.fileOffset[1] << "]" << std::endl;
       }
    }

    ReplayLog replayLog;
    int ReplayLog::addToLog(server::ClientRequestId client_request_id) {
        // check if the there is a client entry
        std::string identifier = client_request_id.ip() + ":" + std::to_string(client_request_id.pid());
        std::unordered_map<std::string, 
            replayLogEntry*>::const_iterator result = 
            client_list.find(identifier);
        if (result == client_list.end()) {
            // entry not found
            new_entry_mutex.lock();
            // have another check in here so multiple threads don't end up adding things twice
            /* the most common case(with an existing client entry) 
             * will no go through this double check logic*/
            result = client_list.find(identifier);
            if (result == client_list.end()) {
                // add the new client entry
                replayLogEntry *new_entry = new replayLogEntry();

                new_entry->timestamp_list.insert(
                    std::make_pair(client_request_id.timestamp(), false));
                client_list.insert(std::make_pair(identifier, new_entry));
                new_entry_mutex.unlock();
                return 0; // first entry for a client added successfully
            }
            new_entry_mutex.unlock();
        }

        // check the particular client entry
        replayLogEntry *client_entry = result->second;

        // first check if the timestamp is and old timestamp
        if (client_entry->timestamp_list.size() > 0) {
            std::map<google::protobuf::Timestamp, bool, Tables::googleTimestampComparator>::iterator it;
            it = client_entry->timestamp_list.begin();
            Tables::googleTimestampComparator comp;
            bool smaller_than_min = comp.operator()(client_request_id.timestamp(), (*it).first);
            if (smaller_than_min) return -2; //acked, smallest timestamp present in the list is greater
        }
        client_entry->client_entry_mutex.lock();

        // add the timestamp
        std::pair<std::map<
            google::protobuf::Timestamp,
            bool,
            Tables::googleTimestampComparator
            >::iterator,bool> insert_result = 
            (client_entry->timestamp_list).insert(
                std::make_pair(client_request_id.timestamp(), false));
        client_entry->client_entry_mutex.unlock();

        if (insert_result.second) {
            // imply the insert is successful and there is no duplicate entry
            return 0;
        }

       return -1;// existing entry
    }

    int ReplayLog::ackLogEntry(server::ClientRequestId client_request_id) {
        // locate the client entry
        std::string identifier = client_request_id.ip() + ":" + std::to_string(client_request_id.pid());
        std::unordered_map<std::string, 
            replayLogEntry*>::const_iterator replay_log_it = 
            client_list.find(identifier);
        if (replay_log_it == client_list.end()) return -1;
        replayLogEntry *client_entry = replay_log_it->second;

        // locate the timestamp entry
        client_entry->client_entry_mutex.lock();
        std::map<google::protobuf::Timestamp,
            bool, Tables::googleTimestampComparator>::iterator timestamp_it = 
            client_entry->timestamp_list.find(client_request_id.timestamp());

        if (timestamp_it == client_entry->timestamp_list.end()) {
            client_entry->client_entry_mutex.unlock();
            return -1;
        }
        
        if (!timestamp_it->second) {
            client_entry->client_entry_mutex.unlock();
            return -2;
        }

        // remove the timestamp entry if it is committed
        client_entry->timestamp_list.erase(timestamp_it);
        client_entry->client_entry_mutex.unlock();
        return -1;
    }
    
    void ReplayLog::cleanOldLogEntry(time_t age) {

    }

    void ReplayLog::printRelayLogContent() {
        for (auto client_entry_pair : client_list) {
            std::cout << "for client " << client_entry_pair.first << std::endl;
            replayLogEntry *client_entry = client_entry_pair.second;
            std::map<google::protobuf::Timestamp, bool, Tables::googleTimestampComparator>::iterator it;
            for (it = client_entry->timestamp_list.begin(); it != client_entry->timestamp_list.end(); it++) {
                std::cout << " " << (*it).first.seconds() << ":" << (*it).first.nanos() << std::endl;
            }
        }
   }
};