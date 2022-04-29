#include <iostream>

#include "master.h"

using namespace std;

grpc::Status master::ClientListenerImpl::GetConfig (
    grpc::ServerContext *context,
    const google::protobuf::Empty *request,
    master::GetConfigReply *reply) {

        master::Node *headNode = master::head;
        master::Node *tailNode = master::tail;

        master::ServerIp * reply_head = reply->mutable_head();
        reply_head->set_ip(headNode->ip);
        reply_head->set_port(headNode->port);

        master::ServerIp * reply_tail = reply->mutable_tail();
        reply_tail->set_ip(tailNode->ip);
        reply_tail->set_port(tailNode->port);

        return grpc::Status::OK;
}