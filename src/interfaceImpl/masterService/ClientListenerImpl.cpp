#include <iostream>

#include "master.h"

using namespace std;

grpc::Status master::ClientListenerImpl::GetConfig (
    grpc::ServerContext *context,
    const google::protobuf::Empty *request,
    master::GetConfigReply *reply) {
        // TODO 
        return grpc::Status::OK;
}