#include <iostream>
#include "server.h"

using namespace std;

grpc::Status server::TailServiceImpl::WriteAck (
    grpc::ServerContext *context,
    const server::WriteAckRequest *request,
    google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}

grpc::Status server::TailServiceImpl::Read (
    grpc::ServerContext *context,
    const server::ReadRequest *request,
    server::ReadReply *reply) {
        // TODO 
        return grpc::Status::OK;
}