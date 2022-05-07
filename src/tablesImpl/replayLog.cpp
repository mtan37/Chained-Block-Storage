#include <map>
#include <iostream>
#include <list>

#include "tables.hpp"

namespace Tables {

    ReplayLog replayLog;
    int ReplayLog::addToLog(server::ClientRequestId client_request_id) {
        // check if the there is a client entry
        std::string identifier = client_request_id.ip() + ":" + std::to_string(client_request_id.pid());
        
        // this lock is used to prevent garbage collection to interfer with fetch of client entry
        std::shared_lock lock_shared(gc_entry_mutex);
        
        std::unordered_map<std::string, 
            replayLogEntry*>::const_iterator result = 
            client_list.find(identifier);
        if (result == client_list.end()) {
            // entry not found
            new_entry_mutex.lock();
            // have another check in here so multiple threads don't end up adding things twice
            /* the most common case(with an existing client entry) 
                * will no go through this double check logic */
            result = client_list.find(identifier);
            if (result == client_list.end()) {
                // add the new client entry
                replayLogEntry *new_entry = new replayLogEntry();

                new_entry->timestamp_list.insert(
                    std::make_pair(client_request_id.timestamp(), false));
                client_list.insert(std::make_pair(identifier, new_entry));
                new_entry_mutex.unlock();
                std::shared_lock unlock_shared(gc_entry_mutex);
                return 0; // first entry for a client added successfully
            }
            new_entry_mutex.unlock();
            std::shared_lock unlock_shared(gc_entry_mutex);
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
        std::shared_lock unlock_shared(gc_entry_mutex);

        if (insert_result.second) {
            // imply the insert is successful and there is no duplicate entry
            return 0;
        }

        return -1;// existing entry
    }

    int ReplayLog::ackLogEntry(server::ClientRequestId client_request_id) {
        // locate the client entry
        std::string identifier = client_request_id.ip() + ":" + std::to_string(client_request_id.pid());
        
        // lock this here so we don't end up having a pointer
        // that is deallocated in the garbage collection
        std::shared_lock lock_shared(gc_entry_mutex);
        std::unordered_map<std::string, 
            replayLogEntry*>::const_iterator replay_log_it = 
            client_list.find(identifier);
        if (replay_log_it == client_list.end()) {
            std::shared_lock unlock_shared(gc_entry_mutex);
            return -1;
        }
        replayLogEntry *client_entry = replay_log_it->second;

        // locate the timestamp entry
        client_entry->client_entry_mutex.lock();
        std::map<google::protobuf::Timestamp,
            bool, Tables::googleTimestampComparator>::iterator timestamp_it = 
            client_entry->timestamp_list.find(client_request_id.timestamp());

        if (timestamp_it == client_entry->timestamp_list.end()) {
            client_entry->client_entry_mutex.unlock();
            std::shared_lock unlock_shared(gc_entry_mutex);
            return -1;
        }
        
        if (!timestamp_it->second) {
            client_entry->client_entry_mutex.unlock();
            std::shared_lock unlock_shared(gc_entry_mutex);
            return -2;
        }

        // remove the timestamp entry if it is committed
        client_entry->timestamp_list.erase(timestamp_it);
        client_entry->client_entry_mutex.unlock();
        std::shared_lock unlock_shared(gc_entry_mutex);
        return 0;
    }

    int ReplayLog::commitLogEntry(server::ClientRequestId client_request_id) {
        // locate the client entry
        std::string identifier = client_request_id.ip() + ":" + std::to_string(client_request_id.pid());
        std::unordered_map<std::string, 
            replayLogEntry*>::const_iterator replay_log_it = 
            client_list.find(identifier);
        if (replay_log_it == client_list.end()) return -1;
        replayLogEntry *client_entry = replay_log_it->second;

        // locate the timestamp entry
        // Then modifies the commit flag. This section is NOT LOCKED
        // since we assume no two thread will try to commit the same request
        // at the same time
        // And the commit try should be recent enough that it should not be affected by removal or garbage collection
        std::map<google::protobuf::Timestamp,
            bool, Tables::googleTimestampComparator>::iterator timestamp_it = 
            client_entry->timestamp_list.find(client_request_id.timestamp());

        if (timestamp_it == client_entry->timestamp_list.end()) return -1;
        if (timestamp_it->second) return -2; // already committed
        // mark the timestamp as committed
        timestamp_it->second = true;
        return 0;
    }

    void ReplayLog::cleanOldLogEntry(time_t age) {
        struct timeval now;
        gettimeofday(&now, NULL);

        // iterate through all the client entries
        // can't insert new entires while iterating through the map
        std::shared_lock lock(gc_entry_mutex);

        // initiating a list of identifier to remove
        std::list<std::string> to_remove;
        for (auto client_entry_pair : client_list) {
            replayLogEntry *client_entry = client_entry_pair.second;
            if(client_entry->timestamp_list.empty()) continue;

            // there will be clock skew between client timestamp and the server's
            // it should be fine though since we are just cleaning out old entires
            google::protobuf::Timestamp timestamp = (--(client_entry->timestamp_list.end()))->first;
            if (timestamp.seconds() < now.tv_sec) {
                time_t diff = now.tv_sec - timestamp.seconds();
                if (diff > age) {
                    // add the identifier to the list to be removed later
                    to_remove.push_back(client_entry_pair.first);
                }
            }
        }

        for (auto identifier : to_remove) {
            std::unordered_map<std::string, 
            replayLogEntry*>::const_iterator replay_log_it = 
                client_list.find(identifier);
            if (replay_log_it == client_list.end()) {
                std::cout << "Something fishy happened. Can't find identifier "
                    << identifier << " in replay log" << std::endl;
                continue;
            }
            replayLogEntry *client_entry = replay_log_it->second;
            client_list.erase(replay_log_it);
            delete client_entry;
        }

        std::shared_lock unlock(gc_entry_mutex);
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

    void ReplayLog::initRelayLogContent(server::UpdateReplayLogRequest content) {
        // TODO
    }

    void ReplayLog::getRelayLogContent(server::UpdateReplayLogRequest &content) {
        // TODO
    }
}