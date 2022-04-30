#include <iostream>
#include "server.h"
#include "tables.hpp"

#include <grpc++/grpc++.h>

// the grpc interface imports
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"

using namespace std;

grpc::Status server::NodeListenerImpl::RelayWrite (grpc::ServerContext *context,
    const server::RelayWriteRequest *request,
    google::protobuf::Empty *reply) {
        
        Tables::PendingQueue::pendingQueueEntry entry;
        entry.seqNum = request->seqnum();
        entry.volumeOffset = request->offset();
        entry.data = request->data();
        //TODO: Include clientID in RelayWrite requests?
        
        Tables::pendingQueue.pushEntry(entry);
        
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::RelayWriteAck (grpc::ServerContext *context,
    const server::RelayWriteAckRequest *request,
    google::protobuf::Empty *reply) {
        
        //TODO: Update metadata table / commit
        //TODO: Remove from sent list, depends on changing sentList data strcuture
        Tables::currentSeq = (int) request->seqnum();
        //TODO: Get next node's sequence number
        Tables::nextSeq = 0;
        
        //Relay to previous nodes
        string prev_node_address = server::upstream->ip + ":" + to_string(server::upstream->port);
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(prev_node_address, grpc::InsecureChannelCredentials(), args);
        std::unique_ptr<server::NodeListener::Stub> next_node_stub = server::NodeListener::NewStub(channel);
        grpc::ClientContext relay_context;
        google::protobuf::Empty RelayWriteAckReply;
        
        grpc::Status status = next_node_stub->RelayWriteAck(&relay_context, *request, &RelayWriteAckReply);

        if (!status.ok()) {
            cout << "Failed to relay write ack to next node" << endl;
            //TODO: Retry?
        }
        
        
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::ReplayLogChange (grpc::ServerContext *context,
        const server::ReplayLogChangeRequest *request,
        google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}
