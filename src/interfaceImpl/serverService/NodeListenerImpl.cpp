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
        cout << "RelayWrite called" << endl;
        //Add to replay log
        int addResult = Tables::replayLog.addToLog(request->clientrequestid());

        if (addResult < 0) {return grpc::Status::OK;}// means entry already exist in log or has been acked
        
        Tables::PendingQueue::pendingQueueEntry entry;
        entry.seqNum = request->seqnum();
        entry.volumeOffset = request->offset();
        entry.data = request->data();
        entry.reqId = request->clientrequestid();
        
        cout << "Added entry " << request->seqnum() << " to pendingqueue with reqId " << entry.reqId.ip() << ":" << entry.reqId.pid() << ":" << entry.reqId.timestamp().seconds() << endl;
        
        Tables::pendingQueue.pushEntry(entry);
        cout << "RelayWrite finished" << endl;
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::RelayWriteAck (grpc::ServerContext *context,
    const server::RelayWriteAckRequest *request,
    google::protobuf::Empty *reply) {
        cout << "RelayWriteAck called" << endl;
        while (Tables::commitSeq + 1 != request->seqnum()) {}
        //Remove from sent list
        Tables::SentList::sentListEntry sentListEntry = Tables::sentList.popEntry((int) request->seqnum());
        Storage::commit(request->seqnum(), sentListEntry.volumeOffset);
                
        int result = Tables::replayLog.commitLogEntry(sentListEntry.reqId);
        cout << "Committed entry " << request->seqnum() << " with reqId " << sentListEntry.reqId.ip() << ":" << sentListEntry.reqId.pid() << ":" << sentListEntry.reqId.timestamp().seconds() << " with result " << result << endl;
        Tables::replayLog.printRelayLogContent();     
           
        Tables::commitSeq = (int) request->seqnum();
        
        cout << "RelayWriteAck checkpoint 1" << endl;
        
        while (server::state != HEAD && server::state != SINGLE) {
            //Relay to previous nodes
            grpc::ClientContext relay_context;
            google::protobuf::Empty RelayWriteAckReply;
            
            grpc::Status status = server::upstream->stub->RelayWriteAck(&relay_context, *request, &RelayWriteAckReply);

            if (status.ok()) break;
            sleep(1);
        }
        cout << "RelayWriteAck checkpoint 2" << endl;
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::ReplayLogChange (grpc::ServerContext *context,
        const server::ReplayLogChangeRequest *request,
        google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::Restore (grpc::ServerContext *context,
        const server::RestoreRequest *request,
        google::protobuf::Empty *reply) {
        
        for (int i = 0; i < request->entry_size(); i++) {
            server::RestoreEntry entry = request->entry(i);
            Storage::write(entry.data(), entry.offset(), entry.seqnum());
            Tables::writeSeq = entry.seqnum();
            Storage::commit(entry.seqnum(), entry.offset());
            Tables::commitSeq = entry.seqnum();
            Tables::currentSeq = entry.seqnum() + 1;
        }
        return grpc::Status::OK;       
}

grpc::Status server::NodeListenerImpl::UpdateReplayLog (grpc::ServerContext *context,
        const server::UpdateReplayLogRequest *request,
        google::protobuf::Empty *reply) {
        Tables::replayLog.initRelayLogContent(*request);
        return grpc::Status::OK;
}