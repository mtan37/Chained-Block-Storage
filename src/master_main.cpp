#include <iostream>

// the grpc interface imports
#include <grpc++/grpc++.h>
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"
#include <google/protobuf/empty.pb.h>
#include "constants.hpp"
#include "master.h"
#include <thread>

using namespace std;

namespace master {
  std::list<master::Node*> nodeList;
  master::Node *head;
  master::Node *tail;
};

// Run grpc service in a loop
void run_service(grpc::Server *server, std::string serviceName) {
  std::cout << "Starting to run " << serviceName << "\n";
  server->Wait();
}

void run_heartbeat(){

}

int main(int argc, char const *argv[]) {

    // Launch master nodeListner to listen for node registration from chain servers
    std::string my_address("0.0.0.0:" + Constants::MASTER_PORT);
    master::NodeListenerImpl nodeListenerService;
    grpc::ServerBuilder nodeListenerBuilder;
    nodeListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    nodeListenerBuilder.RegisterService(&nodeListenerService);
    std::unique_ptr<grpc::Server> nodeListener(nodeListenerBuilder.BuildAndStart());
    // Thread server out and start listening
    std::thread nodeListener_service_thread(run_service, nodeListener.get(), "NodeListener");

    // Launch client listener, replies to client with chain details (head, tail)
    master::ClientListenerImpl clientListenerImpl;
    grpc::ServerBuilder clientListenerBuilder;
    clientListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    clientListenerBuilder.RegisterService(&clientListenerImpl);
    std::unique_ptr<grpc::Server> clientListener(clientListenerBuilder.BuildAndStart());

    std::thread clientListener_service_thread(run_service, clientListener.get(), "ClientListener");

    // Need to launch heartbeat communication with each registered node
    nodeListener_service_thread.join();
    return 0;
}