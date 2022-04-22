#include <iostream>
#include "master.h"

using namespace std;


grpc::Status master::NodeListenerImpl::Register (
    grpc::ServerContext *context,
    const master::RegisterRequest *request,
    master::RegisterReply *reply) {
        // TODO 
        return grpc::Status::OK;
}