#include "server.grpc.pb.h"
using namespace std;
#pragma once

namespace server {
    class MasterListenerImpl;
    class HeadServiceImpl;
    class TailServiceImpl;
    class NodeListenerImpl;
    extern std::string next_node_ip;
    extern std::string next_node_port;
    extern std::string prev_node_ip;
    extern std::string prev_node_port;
    enum State { HEAD, TAIL, MIDDLE, SINGLE };
    extern State state;
}

class server::MasterListenerImpl final : public server::MasterListener::Service {
public:

    grpc::Status HeartBeat (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          google::protobuf::Empty *reply);

    grpc::Status ChangeMode (grpc::ServerContext *context,
                          const server::ChangeModeRequest *request,
                          server::ChangeModeReply *reply);
};

class server::HeadServiceImpl final : public server::HeadService::Service {
public:

    grpc::Status Write (grpc::ServerContext *context,
                          const server::WriteRequest *request,
                          server::WriteReply *reply);
};

class server::TailServiceImpl final : public server::TailService::Service {
public:

    grpc::Status WriteAck (grpc::ServerContext *context,
                          const server::WriteAckRequest *request,
                          google::protobuf::Empty *reply);

    grpc::Status Read (grpc::ServerContext *context,
                          const server::ReadRequest *request,
                          server::ReadReply *reply);
};

class server::NodeListenerImpl final : public server::NodeListener::Service {
public:

    grpc::Status RelayWrite (grpc::ServerContext *context,
                          const server::RelayWriteRequest *request,
                          google::protobuf::Empty *reply);

    grpc::Status RelayWriteAck (grpc::ServerContext *context,
                          const server::RelayWriteAckRequest *request,
                          google::protobuf::Empty *reply);

    grpc::Status ReplayLogChange (grpc::ServerContext *context,
            const server::ReplayLogChangeRequest *request,
            google::protobuf::Empty *reply);             
};