#include <iostream>
#include "server.h"

using namespace std;

grpc::Status WriteAck (
    grpc::ServerContext *context,
    const server::WriteAckRequest *request,
    google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}