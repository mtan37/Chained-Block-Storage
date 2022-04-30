#include <queue>
#include <map>

#include "tables.hpp"

namespace Tables {
    SentList sentList;

    /**
     * @brief Convert clientId object into string so it could
     * be used as map key
     * 
     */
    std::string SentList::clientIdToIdentifier(server::ClientRequestId clientId) {
        google::protobuf::Timestamp timestamp = clientId.timestamp();
        std::string identifier = 
            clientId.ip() + ":" + std::to_string(clientId.pid()) + 
            ":" + std::to_string(timestamp.seconds()) + ":" + 
            std::to_string(timestamp.nanos());
        return identifier; 
    }

    /**
     * adds entry to sentList at clientId
     */
    void SentList::pushEntry(server::ClientRequestId clientId, sentListEntry entry){
        list_mutex.lock();
        auto ret = this->list.insert(std::make_pair(clientIdToIdentifier(clientId), entry));
        list_mutex.unlock();

        if (ret.second == false) {
           throw std::invalid_argument("Key already in list");
        }
    }

    /**
    * Remove single item from sentList, returns error if key not found
    */
     SentList::sentListEntry SentList::popEntry(server::ClientRequestId clientId){
        list_mutex.lock();
        if (getListSize()==0) {
            list_mutex.unlock();
            throw std::length_error("Trying to get key from empty list");
        }

        auto it = this->list.find(clientIdToIdentifier(clientId));
        if (it != this->list.end()){
            SentList::sentListEntry entry = it->second;
            this->list.erase(it);
            list_mutex.unlock();
            return entry;
        } else {
           // throw error
           list_mutex.unlock();
           throw std::invalid_argument("Key not found in sentList");
        }
    }
   
    int SentList::getListSize() {
       return this->list.size();
    }

    void SentList::printSentList() {
        list_mutex.lock();
        for (auto it=this->list.begin(); it!=this->list.end(); ++it){
            std::cout << "    [key:file offsetA,B] = [" << it->first << "][" <<
            it->second.fileOffset[0] << ", " <<
            it->second.fileOffset[1] << "]" << std::endl;
        }
        list_mutex.unlock();
    }
};