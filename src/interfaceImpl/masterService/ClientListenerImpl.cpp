#include <iostream>

#include "master.h"

using namespace std;

grpc::Status master::ClientListenerImpl::GetConfig (
    grpc::ServerContext *context,
    const google::protobuf::Empty *request,
    master::GetConfigReply *reply) {
        cout<<"getConfig call" << endl;
        master::Node *headNode = master::head;
        master::Node *tailNode = master::tail;

        master::ServerIp * reply_head = reply->mutable_head();
        if (!headNode) {
            reply_head->set_ip("localhost");
            reply_head->set_port(-1);
        } else {
            reply_head->set_ip(headNode->ip);
            reply_head->set_port(headNode->port);
        }

        master::ServerIp * reply_tail = reply->mutable_tail();
        if (!tailNode) {
            reply_head->set_ip("localhost");
            reply_head->set_port(-1);
        } else {
            reply_tail->set_ip(tailNode->ip);
            reply_tail->set_port(tailNode->port);
        }

        return grpc::Status::OK;
}