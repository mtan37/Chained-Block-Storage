#include <iostream>

#include "master.h"

using namespace std;

grpc::Status master::ClientListenerImpl::GetConfig (
    grpc::ServerContext *context,
    const google::protobuf::Empty *request,
    master::GetConfigReply *reply) {
//
//        master::ServerIp head;
//        master::Node headNode = master::nodeList.front();
//        head->setIp(headNode.ip);
//        head->setPort(headNode.port);
//
//        master::ServerIp tail;
//        master::Node tailNode = master::nodeList.back();
//        tail.setIp(tailNode.ip);
//        tail.setPort(tailNode.port);
//
//        reply->setHead(head);
//        reply->setTail(tail);
//
//        return grpc::Status::OK;
}