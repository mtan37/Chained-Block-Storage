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

    string print_state(server::State state) {
        switch (state) {
            case server::HEAD:
                return "Head";
                break;
            case server::TAIL:
                return "Tail";
                break;
            case server::MIDDLE:
                return "Mid";
                break;
            case server::SINGLE:
                return "Single Server";
                break;
            case server::INITIALIZE:
                return "Initializing";
                break;
            case server::TRANSITION:
                return "Transitioning";
                break;
        }
    }

    void print_nodes(){
        master::nodeList_mtx.lock();
        std::list<master::Node*> copy_node_list(master::nodeList);
        master::nodeList_mtx.unlock();
        std::list<master::Node*>::iterator it;
        for (it = copy_node_list.begin(); it != copy_node_list.end(); it++){
            if (it != copy_node_list.begin()) cout << "-";
            string ip_final = (*it)->ip;
            int ip_pos = ip_final.rfind(".");
            ip_final = ip_final.substr(ip_pos+1);
            cout << print_state((*it)->state)
                 << "(" << ip_final << ":" << (*it)->port << ")";
        }
        cout << endl;
    }

    void build_node_stub(master::Node* node){
        string node_addr(node->ip + ":" + to_string(node->port));
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
        node->stub = server::MasterListener::NewStub(channel);
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
void hlp_Manage_Failure(std::list<master::Node*>::iterator it, master::Node *current ){
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
        master::nodeList_mtx.lock();
        std::list<master::Node*> copy_node_list(master::nodeList);
        master::nodeList_mtx.unlock();
        if (hb_two) cout << ".." << endl;
        else cout << "." << endl;
        hb_two = !hb_two;
        //
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
                } else if (status.error_code() == grpc::UNIMPLEMENTED) {
                    // Heartbeat can happen as soon as server is registered, but
                    // the server itself may not have listener set up immediately
                    // So comment but keep going
                    cout << "Server not ready to receive heartbeat" << endl;
                    cout << status.error_code() << ": " << status.error_message()  
                         << endl;
                    sleep(2);
                    build_node_stub(current);
                } else {
                    cout << "Something unexpected happened. Shutting down the server\n";
                    cout << status.error_code() << ": " << status.error_message()
                         << endl;
                    exit(1);
                }
            }
        }

        sleep(master::HEARTBEAT);
    }

}

int main(int argc, char const *argv[]) {

    // Launch master listner to listen for node registration from chain servers
    std::string my_address("0.0.0.0:" + Constants::MASTER_PORT);
    master::NodeListenerImpl nodeListenerService;
    grpc::ServerBuilder nodeListenerBuilder;
    nodeListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    nodeListenerBuilder.RegisterService(&nodeListenerService);
    // Thread server out and start listening
    std::unique_ptr<grpc::Server> nodeListener(nodeListenerBuilder.BuildAndStart());
    std::thread nodeListener_service_thread(run_service, nodeListener.get(), "listen to Nodes");

    // Launch client listener, replies to client with chain details (head, tail)
    master::ClientListenerImpl clientListenerImpl;
    grpc::ServerBuilder clientListenerBuilder;
    clientListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    clientListenerBuilder.RegisterService(&clientListenerImpl);
    // Thread server out and start listening
    std::unique_ptr<grpc::Server> clientListener(clientListenerBuilder.BuildAndStart());
    std::thread clientListener_service_thread(run_service, clientListener.get(), "listen to clients");

    // Need to launch heartbeat communication with each registered node
    run_heartbeat();

    /**
     * Start shutdown
     */

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