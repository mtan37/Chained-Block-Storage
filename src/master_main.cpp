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
int hlp_Manage_Failure(std::list<master::Node*>::iterator it){
    master::Node *current = *it;
    master::Node* try_next; // node we are checking as replacement for current
    master::Node* endpoint_node; // If you reach this you are at end\start of list
    cout << "heartbeat failed on " << master::print_state(current->state) << endl;
    std::list<master::Node*> drop_list;
    drop_list.push_back(current);
    // Deal with failure scenarios based on state
    switch (current->state){  // state of failed node
        case (server::SINGLE):
            // Against our policy
            cout << "All nodes have failed" << endl;
            break;
        case (server::HEAD):
        case (server::TAIL):
        {
            // Head and tail are conceptually the same, only direction of movement, and state vary
            // just move through node list until you find next alive node, that node takes over currents
            // state or become single server

            // context and reply
            server::ChangeModeReply cm_reply;
            server::State newState;
            bool keep_going = true;
            // identify end of list
            if (current->state == server::HEAD) endpoint_node = master::tail;
            else endpoint_node = master::head;
            while (keep_going) {
                grpc::ClientContext cm_context;
                cout << "... checking next available node" << endl;
                // set request params for change_mode
                if (current->state == server::HEAD) it++;
                else it--;
                // if at end point, stop.  Either this
                try_next = *(it);
                if (try_next == endpoint_node) keep_going = false;
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
                        cout << "...Error: Something unexpected happened." << endl;
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
        case (server::MIDDLE): {
            cout << "Mid node failed" << endl;
            // Need to identify next operational nodes downstream and upstream before either change mode,
            // because we need to send in appropriate IPs. Once ID'd send change_mode downstream and
            // get last seq number

            // Identify operational nodes in each direction
            google::protobuf::Empty hb_request;
            google::protobuf::Empty hb_reply;
            int DOWN = 0;
            int UP = 1;
            master::Node *nodes[2];
            std::list<master::Node*>::iterator initial_it;
            initial_it = it;
            for (int i = 0; i < 2; i++) {
                // identify end of list
                if (i==UP) it = initial_it;
                bool past_endpoint = false;
                if (i == DOWN) endpoint_node = master::tail;
                else endpoint_node = master::head;
                while (true) {
                    // set below when we hit endpoint, when we come back around this is officialy
                    // an endpoint failure, not a mid failure
                    if (past_endpoint){
                        hlp_Manage_Failure(it);
                        return -1;
                    }
                    grpc::ClientContext hb_context;
                    if (i == DOWN) it++;
                    else it--;
                    try_next = *(it);
                    // if we hit end point, this is endpoint failure, not a mid-failure
                    if (try_next == endpoint_node) past_endpoint = true;
                    grpc::Status hb_status = try_next->stub->HeartBeat(&hb_context, hb_request, &hb_reply);
                    if (!hb_status.ok()) {
                        if (hb_status.error_code() == grpc::UNAVAILABLE) {
                            cout << "...next node unavailable" << endl;
                        } else if (hb_status.error_code() == grpc::UNIMPLEMENTED) {
                            cout << "...next node has not exposed calls" << endl;
                        } else {
                            cout << "...Error: Something unexpected happened." << endl;
                            cout << hb_status.error_code() << ": " << hb_status.error_message()
                                 << endl;
                        }
                        // any failure means it comes of the list for now
                        drop_list.push_back(try_next);
                    } else {
                        nodes[i] = try_next;
                        break;
                    }
                }
            }

            // At this point we should have m-1 (up) and m+1 (down) nodes identified
            // Good to go ahead and run change_mode.  At this point if we encounter failure on
            // change mode then just break out of helper and try again on next HB
            server::ChangeModeRequest down_request;
            down_request.set_new_state(nodes[DOWN]->state); // maintain down stream node state
            server::ServerIp * up_addr = down_request.mutable_prev_addr();
            up_addr->set_ip(nodes[UP]->ip);
            up_addr->set_port(nodes[UP]->port);
            // Other stub members
            server::ChangeModeReply down_reply;
            grpc::ClientContext down_context;
            // Contact downstream, notify of change, get sequence number
            grpc::Status down_status = nodes[DOWN]->stub->ChangeMode(&down_context, down_request, &down_reply);
            if (!down_status.ok()) {
                if (down_status.error_code() == grpc::UNAVAILABLE) {
                    cout << "...downstream node unavailable" << endl;
                } else if (down_status.error_code() == grpc::UNIMPLEMENTED) {
                    cout << "...downstream node has not exposed calls" << endl;
                } else {
                    cout << "...Error: (downstream) Something unexpected happened." << endl;
                    cout << down_status.error_code() << ": " << down_status.error_message()
                         << endl;
                }
                return -1;
            } else {
                // Contact upstream, notify of change, send in sequence number
                // Upstream will communicate with downstream, will not return until nodes
                // are ready to resume communication
                server::ChangeModeRequest up_request;
                up_request.set_new_state(nodes[UP]->state); // maintain up stream node state
                up_request.set_last_seq_num(down_reply.lastreceivedseqnum());
                server::ServerIp * down_addr = up_request.mutable_next_addr();
                down_addr->set_ip(nodes[DOWN]->ip);
                down_addr->set_port(nodes[DOWN]->port);

                // Other stub members
                server::ChangeModeReply up_reply;
                grpc::ClientContext up_context;
                grpc::Status up_status = nodes[UP]->stub->ChangeMode(&up_context, up_request, &up_reply);
                if (!up_status.ok()) {
                    if (up_status.error_code() == grpc::UNAVAILABLE) {
                        cout << "...upstream node unavailable" << endl;
                    } else if (up_status.error_code() == grpc::UNIMPLEMENTED) {
                        cout << "...upstream node has not exposed calls" << endl;
                    } else {
                        cout << "...Error: (up) Something unexpected happened." << endl;
                        cout << up_status.error_code() << ": " << up_status.error_message()
                             << endl;
                    }
                    return -1;
                } else {
                    // success
                    cout << "...Successfully connected m-1 with m+1" << endl;
                }
                break;
            }
            break;
        }
        default:
            cout << "Initializing node failed" << endl;
    }

    // Remove items in droplist from true node_list
    std::list<master::Node*>::iterator drop_it;
    for (drop_it = drop_list.begin(); drop_it != drop_list.end(); drop_it++){
        master::nodeList.remove(*drop_it);
    }
    master::print_nodes();
    return 0;
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
                    int resp = hlp_Manage_Failure(it);
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