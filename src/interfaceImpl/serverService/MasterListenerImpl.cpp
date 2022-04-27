#include <iostream>
#include "server.h"

using namespace std;

grpc::Status server::MasterListenerImpl::HeartBeat (grpc::ServerContext *context,
    const google::protobuf::Empty *request,
    google::protobuf::Empty *reply){
        // TODO 
        return grpc::Status::OK;}

grpc::Status server::MasterListenerImpl::ChangeMode (grpc::ServerContext *context,
    const server::ChangeModeRequest *request,
    server::ChangeModeReply *reply) {
        
        //TODO: Set prev/next node info (don't replace if empty request, case with new tail), change HEAD/MIDDLE/TAIL state
        
        //TODO: Handle sequence numbers and reply
        
        return grpc::Status::OK;
}