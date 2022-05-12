#include "server.grpc.pb.h"
#include <grpc++/grpc++.h>
using namespace std;
#pragma once

namespace server {
    class MasterListenerImpl;
    class HeadServiceImpl;
    class TailServiceImpl;
    class NodeListenerImpl;
    class RecoveryServiceImpl;

    // members
    struct Node {
        std::string ip = "";
        int port = -1;
        std::unique_ptr<server::NodeListener::Stub> stub;
    };
    extern server::Node *downstream;
    extern server::Node *upstream;
    extern std::mutex changemode_mtx;
    enum State { HEAD, TAIL, MIDDLE, SINGLE, INITIALIZE, TRANSITION };
    extern State state;
    extern std::unique_ptr<grpc::Server> tailService;
    extern std::string my_ip;

    // variable related to throughput benchmark
    extern std::mutex benchmark_time_recorder_mtx;
    extern bool does_record;
    extern std::string record_file_name;

    // methods
    extern string get_state(server::State state);
    extern void launch_tail();
    extern void kill_tail();
    extern void launch_head();
    extern void run_service(grpc::Server *, std::string);
    extern void build_node_stub(server::Node* node);
}

class server::MasterListenerImpl final : public server::MasterListener::Service {
public:

//    MasterListenerImpl(){cout << "Preping master" << endl;};
//    ~MasterListenerImpl(){cout << "Closing master" << endl;};
    grpc::Status HeartBeat (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          google::protobuf::Empty *reply);

    grpc::Status ChangeMode (grpc::ServerContext *context,
                          const server::ChangeModeRequest *request,
                          server::ChangeModeReply *reply);
};

class server::HeadServiceImpl final : public server::HeadService::Service {
public:
//    HeadServiceImpl(){cout << "Preping head" << endl;};
//    ~HeadServiceImpl(){cout << "Closing head" << endl;};
    grpc::Status Write (grpc::ServerContext *context,
                          const server::WriteRequest *request,
                          server::WriteReply *reply);

    grpc::Status ChecksumSystem (grpc::ServerContext *context,
                        const google::protobuf::Empty *request,
                        server::ChecksumReply *reply);
};

class server::TailServiceImpl final : public server::TailService::Service {
public:

//    TailServiceImpl() {cout << "Preparing tail service" << endl;};
//    ~TailServiceImpl(){cout << "Closing tail service" << endl;};
    grpc::Status WriteAck (grpc::ServerContext *context,
                          const server::WriteAckRequest *request,
                          server::WriteAckReply *reply);

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
        
    grpc::Status Restore (grpc::ServerContext *context,
            const server::RestoreRequest *request,
            google::protobuf::Empty *reply);  
            
    grpc::Status UpdateReplayLog (grpc::ServerContext *context,
            const server::UpdateReplayLogRequest *request,
            google::protobuf::Empty *reply);

    grpc::Status ChecksumChain (grpc::ServerContext *context,
                               const server::ChecksumReply *request,
                               server::ChecksumReply *reply);
                               
    grpc::Status AckReplayLog (grpc::ServerContext *context,
                                      const server::AckReplayLogRequest *request,
                                      google::protobuf::Empty *reply);
};

void relay_write_ack_background();