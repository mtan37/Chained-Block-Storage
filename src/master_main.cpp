#include <iostream>
#include <thread>
#include <mutex>

// the grpc interface imports
#include <grpc++/grpc++.h>
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"
#include <google/protobuf/empty.pb.h>
#include "constants.hpp"
#include "master.h"


using namespace std;

namespace master {
    std::list<master::Node*> nodeList;
    master::Node *head;
    master::Node *tail;
    std::mutex nodeList_mtx;

    void print_nodes(){

    }
};

// Run grpc service in a loop
void run_service(grpc::Server *server, std::string serviceName) {
  std::cout << "Starting to " << serviceName << "\n";
  server->Wait();
}

/**
 * Change Node Scenerios
 *  a) Normal operations
 *      i. Single server, new tail, become head
 *      ii. tail, new tail, need to stay tail, but start bringing pending tail up to speed
 *  b) Failures
 *      i. Head, 2 nodes, tail fails, become single server
 *      ii. Tail, 2 nodes, head fails, become single server
 *      iii. M-1\M+1, 3+ nodes, mid fails, +1 and -1 need to converse, state stays the same
 *      iv.  Mid, 3+ nodes, head fails, become new head
 *      v. Mid, 3+ nodes, tail fails, become the new tail
 */
void hlp_Manage_Failure(){
    // Need to deal with failure
    //TODO How do we deal with multiple nodes failing at the same time?
    //TODO Are we going to allow for N-1 failures, but say only 1 at a time?
    cout << "Connection dropped" << endl;
    if (current == master::head){
        // Head failure
        cout << "Head failed" << endl;
    } else if (current == master::tail) {
        cout << "Tail failed" << endl;
    } else {
        // Could actually be mid, or new
        cout << "Other node failed" << endl;
    }

}
/**
 * Normal operations, just ensures no nodes have gone down
 * On failure detection
 *  a)
 *  b)
 */
void run_heartbeat(){
    google::protobuf::Empty request;
    google::protobuf::Empty reply;
    bool hb_two = false;
    while (true){
        if (hb_two) cout << ".." << endl;
        else cout << "." << endl;
        hb_two = !hb_two;
        //
        master::nodeList_mtx.lock();
        std::list<master::Node*> copy_node_list(master::nodeList);
        master::nodeList_mtx.unlock();
        std::list<master::Node*>::iterator it;

        master::Node *current;
        master::Node *prev;
        for (it = copy_node_list.begin(); it != copy_node_list.end(); it++){
            current = *it;
//            cout << "About to call heartbeat on node " << current->port << endl;
            grpc::ClientContext context;
            grpc::Status status = current->stub->HeartBeat(&context, request, &reply);
            if (!status.ok()){
                if (status.error_code() == grpc::UNAVAILABLE) {
                    // If we do need to reconfigure how do we deal with it?
                    hlp_Manage_Failure(it, current);
                } else {
                    cout << "Something unexpected happend. Shuting down the server\n";
                    cout << status.error_code() << ": " << status.error_message()
                         << endl;
                    quit();
                }
            }
        }

        sleep(master::HEARTBEAT);
    }

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
    std::thread nodeListener_service_thread(run_service, nodeListener.get(), "Listen to Nodes");

    // Launch client listener, replies to client with chain details (head, tail)
    master::ClientListenerImpl clientListenerImpl;
    grpc::ServerBuilder clientListenerBuilder;
    clientListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    clientListenerBuilder.RegisterService(&clientListenerImpl);
    std::unique_ptr<grpc::Server> clientListener(clientListenerBuilder.BuildAndStart());

    std::thread clientListener_service_thread(run_service, clientListener.get(), "ClientListener");

    // Need to launch heartbeat communication with each registered node
    // Heartbeat is fine with one node, but with two fails on the second node for some reason
    run_heartbeat();
    nodeListener_service_thread.join();

    // free memory
    master::nodeList_mtx.lock();
    std::list<master::Node*>::iterator it;
    it = master::nodeList.begin();
    while (it != master::nodeList.end()){
        delete *it;
        master::nodeList.erase(it);
    }
    master::nodeList_mtx.unlock();


    return 0;
}