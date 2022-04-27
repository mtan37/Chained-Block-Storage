#include "master.grpc.pb.h"
#include <list>
#pragma once

namespace master {
    class NodeListenerImpl;
    class ClientListenerImpl;
    
    struct Node {
      std::string ip;
      std::string port;
    };
    extern std::list<Node> nodeList;
}

class master::NodeListenerImpl final : public master::NodeListener::Service {
public:

  grpc::Status Register (grpc::ServerContext *context,
                          const master::RegisterRequest *request,
                          master::RegisterReply *reply);
};

class master::ClientListenerImpl final : public master::ClientListener::Service {
public:

  grpc::Status GetConfig (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          master::GetConfigReply *reply);
};

