#include <iostream>

// the grpc interface imports
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"
#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include "master.h"
#include <thread>

using namespace std;

namespace master {
  std::list<master::Node> nodeList;
};

// Run grpc service in a loop
void run_service(grpc::Server *server, std::string serviceName) {
  std::cout << "Starting to run " << serviceName << "\n";
  server->Wait();
}

int main(int argc, char const *argv[]) {
    
    master::NodeListenerImpl nodeListenerImpl;
    std::string my_address = std::string(" ") + std::string(":") + std::string(" ");
    grpc::ServerBuilder nodeListenerBuilder;
    nodeListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    nodeListenerBuilder.RegisterService(&nodeListenerImpl);
    std::unique_ptr<grpc::Server> nodeListener(nodeListenerBuilder.BuildAndStart());
    
    std::thread nodeListener_service_thread(run_service, nodeListener.get(), "NodeListener");
    
    master::ClientListenerImpl clientListenerImpl;
    grpc::ServerBuilder clientListenerBuilder;
    clientListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    clientListenerBuilder.RegisterService(&clientListenerImpl);
    std::unique_ptr<grpc::Server> clientListener(clientListenerBuilder.BuildAndStart());
    
    std::thread clientListener_service_thread(run_service, clientListener.get(), "ClientListener");
    
    return 0;
}