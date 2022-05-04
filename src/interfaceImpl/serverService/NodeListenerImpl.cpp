#include <iostream>
#include "server.h"
#include "tables.hpp"
#include "storage.hpp"

#include <grpc++/grpc++.h>

// the grpc interface imports
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"

using namespace std;

grpc::Status server::NodeListenerImpl::RelayWrite (grpc::ServerContext *context,
    const server::RelayWriteRequest *request,
    google::protobuf::Empty *reply) {
        
        //Add to replay log
        int addResult = Tables::replayLog.addToLog(request->clientrequestid());

        if (addResult < 0) {return grpc::Status::OK;}// means entry already exist in log or has been acked
        
        Tables::PendingQueue::pendingQueueEntry entry;
        entry.seqNum = request->seqnum();
        entry.volumeOffset = request->offset();
        entry.data = request->data();
        
        Tables::pendingQueue.pushEntry(entry);
        
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::RelayWriteAck (grpc::ServerContext *context,
    const server::RelayWriteAckRequest *request,
    google::protobuf::Empty *reply) {
                
        //Remove from sent list
        Tables::SentList::sentListEntry sentListEntry = Tables::sentList.popEntry((int) request->seqnum());
        Storage::commit(request->seqnum(), sentListEntry.volumeOffset);
        Tables::replayLog.commitLogEntry(request->clientrequestid());
                
        Tables::commitSeq = (int) request->seqnum();
        
        while (server::state != HEAD) {
            //Relay to previous nodes
            grpc::ClientContext relay_context;
            google::protobuf::Empty RelayWriteAckReply;
            
            grpc::Status status = server::upstream->stub->RelayWriteAck(&relay_context, *request, &RelayWriteAckReply);

            if (status.ok()) break;
        }
        
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::ReplayLogChange (grpc::ServerContext *context,
        const server::ReplayLogChangeRequest *request,
        google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}
