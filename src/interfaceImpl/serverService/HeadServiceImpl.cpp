#include <iostream>
#include "server.h"

using namespace std;

grpc::Status server::HeadServiceImpl::Write (
    grpc::ServerContext *context,
    const server::WriteRequest *request,
    server::WriteReply *reply) {
        // TODO 
        return grpc::Status::OK;
}