#include <iostream>
#include "master.grpc.pb.h"
using namespace std;
#pragma once

class NodeListenerImpl final : public master::NodeListener::Service {
public:

  grpc::Status Register (grpc::ServerContext *context,
                          const master::RegisterRequest *request,
                          master::RegisterReply *reply);
};

class ClientListenerImpl final : public master::ClientListener::Service {
public:

  grpc::Status GetConfig (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          master::GetConfigReply *reply);
};

