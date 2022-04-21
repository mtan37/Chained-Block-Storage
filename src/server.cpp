#include <iostream>
#include <grpc++/grpc++.h>

// the grpc interface imports
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"

#include "constants.hpp"

using namespace std;

// Global variables
string master_ip;

class NodeListenerImpl final : public master::NodeListener::Service {
public:

  grpc::Status Register (grpc::ServerContext *context,
                          const master::RegisterRequest *request,
                          master::RegisterReply *reply) {
    //return success
    return grpc::Status::OK;
  }
};

/**
 Parse out arguments sent into program
 -alt = secondary server ip addy & port
 -listn = what port we want to listen on
 */
int parse_args(int argc, char** argv){    
    if (argc < 2) {
        cout << "Usage: server <master ip> \n"; 
        return -1;
    }

    master_ip = string(argv[1]);
    return 0;
}

int register_server() {
    // call master to register self
    string master_address = master_ip + ":" + Constants::MASTER_PORT;
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
    std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(master_address, grpc::InsecureChannelCredentials(), args);
    std::unique_ptr<master::NodeListener::Stub> master_stub = master::NodeListener::NewStub(channel);
    grpc::ClientContext context;
    master::RegisterRequest request;
    master::RegisterReply reply;
    grpc::Status status = master_stub->Register(&context, request, &reply);

    if (!status.ok()) {
        cout << "Can't contact master at " << master_address << endl;
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (parse_args(argc, argv) < 0) return -1;
    if (register_server() < 0) return -1;
    return 0;
}