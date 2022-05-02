#include "master.grpc.pb.h"
#include <grpc++/grpc++.h>
#include <list>
#include "server.h"

#pragma once

namespace master {
    class NodeListenerImpl;
    class ClientListenerImpl;

    // members
    struct Node {
      std::string ip;
      int port;
      server::State state = server::INITIALIZE;
      std::unique_ptr<server::MasterListener::Stub> stub;
    };
    const int HEARTBEAT = 5;
    extern std::list<Node*> nodeList;
    extern master::Node *head;
    extern master::Node *tail;
    extern std::mutex nodeList_mtx;

    // methods
    extern void print_nodes();
    extern void build_node_stub(master::Node* node);
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

