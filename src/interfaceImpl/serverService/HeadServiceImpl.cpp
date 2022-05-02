#include <iostream>
#include "server.h"
#include "tables.hpp"

using namespace std;

grpc::Status server::HeadServiceImpl::Write (
    grpc::ServerContext *context,
    const server::WriteRequest *request,
    server::WriteReply *reply) {
        
        // TODO: Check replay log 
        
        Tables::PendingQueue::pendingQueueEntry entry;
        entry.seqNum = Tables::writeSeq;
        entry.volumeOffset = request->offset();
        entry.data = request->data();
        //TODO: Include clientID in RelayWrite requests?
        
        Tables::pendingQueue.pushEntry(entry);
        
        reply->set_seqnum(Tables::writeSeq);
        Tables::writeSeq++;
        
        return grpc::Status::OK;
}