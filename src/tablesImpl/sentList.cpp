#include <queue>
#include <map>

#include "tables.hpp"

namespace Tables {
    SentList sentList;

    /**
     * adds entry to sentList at seqNum
     */
    void SentList::pushEntry(int seqNum, sentListEntry entry){
        list_mutex.lock();
        auto ret = this->list.insert(std::make_pair(seqNum, entry));
        list_mutex.unlock();

        if (ret.second == false) {
           throw std::invalid_argument("Key already in list");
        }
    }

    /**
    * Remove single item from sentList, returns error if key not found
    */
     SentList::sentListEntry SentList::popEntry(int seqNum){
        list_mutex.lock();
        if (getListSize()==0) {
            list_mutex.unlock();
            throw std::length_error("Trying to get key from empty list");
        }

        auto it = this->list.find(seqNum);
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

    std::list<SentList::sentListEntry> SentList::popSentListRange(int startSeqNum) {
        std::list<sentListEntry> returnList;
        std::map<int, sentListEntry>::iterator it;

        list_mutex.lock();
        // Return if list is empty
        if (getListSize()==0) {
            list_mutex.unlock();
            throw std::length_error("Trying to get range from empty list");
        }

        // Set iterator to correct location based on startSeqNum
        if (startSeqNum == 0) it = this->list.begin();
        else {
            it = this->list.find(startSeqNum);
            if (it == this->list.end()){
                list_mutex.unlock();
                throw std::invalid_argument("Key not found in sentList");
            }
        }

        while(it != this->list.end()){
            sentListEntry returnItem;
            returnItem = it->second;
            returnList.push_back(returnItem);
            it = this->list.erase(it);
        }
        list_mutex.unlock();
        return returnList;
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