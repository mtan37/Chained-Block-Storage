#include <iostream>
#include "server.h"
#include "tables.hpp"

using namespace std;

grpc::Status server::NodeListenerImpl::RelayWrite (grpc::ServerContext *context,
    const server::RelayWriteRequest *request,
    google::protobuf::Empty *reply) {
        // TODO 
        //Test global tables
        //Tables::pendingQueueEntry entry;
        //Tables::pushPendingQueue(entry);
        
        
        
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::RelayWriteAck (grpc::ServerContext *context,
    const server::RelayWriteAckRequest *request,
    google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::ReplayLogChange (grpc::ServerContext *context,
        const server::ReplayLogChangeRequest *request,
        google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}