#include "master.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <list>
#include "server.h"

#pragma once

namespace master {
    class NodeListenerImpl;
    class ClientListenerImpl;
    
    struct Node {
      std::string ip;
      int port;
      std::unique_ptr<server::MasterListener::Stub> stub;
    };
    extern std::list<Node*> nodeList;
    extern master::Node *head;
    extern master::Node *tail;
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

