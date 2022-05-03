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
  std::cout << "Will no longer " << serviceName << "\n";
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
int hlp_Manage_Failure(std::list<master::Node*>::iterator it, master::Node *current, std::list<master::Node*>* nList){
    master::Node* try_next;
    int move_it = 0;
    server::State end_state;
    master::Node* update_node;
    master::Node* stop_at_node;
    bool run_endpoint = false;
    //
    cout << "heartbeat failed on " << master::print_state(current->state) << endl;
    std::list<master::Node*> drop_list;
    drop_list.push_back(current);
    //
    switch (current->state){  // state of failed node
        case (server::SINGLE):
            // Against our policy
            cout << "All nodes have failed" << endl;
            break;
        case (server::HEAD):
        case (server::TAIL):
        {
            // context and reply
            server::ChangeModeReply cm_reply;
            server::State newState;
            bool keep_going = true;
            // identify end of list
            if (current->state == server::HEAD) stop_at_node = master::tail;
            else stop_at_node = master::head;
            while (keep_going) {
                grpc::ClientContext cm_context;
                cout << "... checking next available node" << endl;
                // set request params for change_mode
                if (current->state == server::HEAD) it++;
                else it--;
                // if at end point, stop.  Either this
                try_next = *(it);
                if (try_next == stop_at_node) keep_going = false;
                // setup request details, only need to let it know of new status as head
                server::ChangeModeRequest cm_request;
                if (!keep_going) newState = server::SINGLE; // hit end of list, only single node left
                else newState = current->state;

                cm_request.set_new_state(newState);
                // Attempt to update next node
                grpc::Status status = try_next->stub->ChangeMode(&cm_context, cm_request, &cm_reply);
                if (!status.ok()) {
                    if (status.error_code() == grpc::UNAVAILABLE) {
                        cout << "...next node unavailable" << endl;
                    } else if (status.error_code() == grpc::UNIMPLEMENTED) {
                        cout << "...next node has not exposed calls" << endl;
                    } else {
                        cout << "...Error: Something unexpected happened. Aborting registration" << endl;
                        cout << status.error_code() << ": " << status.error_message()
                             << endl;
                    }
                    // any failure means it comes of the list for now
                    drop_list.push_back(try_next);
                } else {
                    try_next->state = newState;
                    if (current->state == server::HEAD) master::head = try_next;
                    else master::tail = try_next;
                    break;
                }
            }
            break;
        }
        case (server::MIDDLE):
            cout << "Mid node failed" << endl;
            break;
        default:
            cout << "Initializing node failed" << endl;
    }
    // deal with head and tail failure
//    if (run_endpoint){
//        // context and reply
//        server::ChangeModeReply cm_reply;
//        server::State newState;
//        bool keep_going = true;
//        // identify end of list
//        if (current->state == server::HEAD) stop_at_node = master::tail;
//        else stop_at_node = master::head;
//        while (keep_going) {
//            grpc::ClientContext cm_context;
//            cout << "... checking next available node" << endl;
//            // set request params for change_mode
//            if (current->state == server::HEAD) it++;
//            else it--;
//            // if at end point, stop.  Either this
//            try_next = *(it);
//            if (try_next == stop_at_node) keep_going = false;
//            // setup request details, only need to let it know of new status as head
//            server::ChangeModeRequest cm_request;
//            if (!keep_going) newState = server::SINGLE; // hit end of list, only single node left
//            else newState = current->state;
//
//            cm_request.set_new_state(newState);
//            // Attempt to update next node
//            grpc::Status status = try_next->stub->ChangeMode(&cm_context, cm_request, &cm_reply);
//            if (!status.ok()) {
//                if (status.error_code() == grpc::UNAVAILABLE) {
//                    cout << "...next node unavailable" << endl;
//                } else if (status.error_code() == grpc::UNIMPLEMENTED) {
//                    cout << "...next node has not exposed calls" << endl;
//                } else {
//                    cout << "...Error: Something unexpected happened. Aborting registration" << endl;
//                    cout << status.error_code() << ": " << status.error_message()
//                         << endl;
//                }
//                // any failure means it comes of the list for now
//                drop_list.push_back(try_next);
//            } else {
//                try_next->state = newState;
//                if (current->state == server::HEAD) master::head = try_next;
//                else master::tail = try_next;
////                update_node = try_next;
//                break;
//            }
//        }
//    }

    // Remove items in droplist from true node_list
    std::list<master::Node*>::iterator drop_it;
//    master::nodeList_mtx.lock();
    for (drop_it = drop_list.begin(); drop_it != drop_list.end(); drop_it++){
        master::nodeList.remove(*drop_it);
    }
    master::print_nodes();
//    master::nodeList_mtx.unlock();
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
        int sleep_time = master::HEARTBEAT;
        master::nodeList_mtx.lock();
        std::list<master::Node*> copy_node_list(master::nodeList);
        master::nodeList_mtx.unlock();
        if (hb_two) cout << ".." << endl;
        else cout << "." << endl;
        hb_two = !hb_two;
        //
        std::list<master::Node*>::iterator it;
        master::Node *current;
//        master::Node *prev;
        for (it = copy_node_list.begin(); it != copy_node_list.end(); it++){
            current = *it;
//            cout << "About to call heartbeat on node " << current->port << endl;
            grpc::ClientContext context;
            grpc::Status status = current->stub->HeartBeat(&context, request, &reply);
            if (!status.ok()){
                if (status.error_code() == grpc::UNAVAILABLE) {
                    // Deal with failure, lock while we correct setup so no new nodes join
//                    master::nodeList_mtx.lock();
                    int resp = hlp_Manage_Failure(it, current, &copy_node_list);
                    // If we get resp of -1 we are out of nodes
                    sleep_time = 0;
                    break;
//                    master::nodeList_mtx.unlock();
                } else if (status.error_code() == grpc::UNIMPLEMENTED) {
                    // element not exposed for some reason
                    cout << "Server not ready to receive heartbeat" << endl;
                    cout << status.error_code() << ": " << status.error_message()  
                         << endl;
//                    build_node_stub(current);
                } else {
                    cout << "Something unexpected happened. Shutting down the server\n";
                    cout << status.error_code() << ": " << status.error_message()
                         << endl;
                    exit(1);
                }
            }
        }
        sleep(sleep_time);
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