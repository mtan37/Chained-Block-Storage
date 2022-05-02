#include <iostream>
#include "master.h"
#include "server.h"
#include <grpc++/grpc++.h>

using namespace std;

/**
 * Nodes need to register with server to get added to the chain. When it registers the
 * master needs to update its nodelist with the new server at back (acting as future new tail)
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status master::NodeListenerImpl::Register (
    grpc::ServerContext *context,
    const master::RegisterRequest *request,
    master::RegisterReply *reply) {

    /**
     * Build and test new node
     */
    master::Node *newNode = new Node;
    master::ServerIp serverIP = request->server_ip();
    newNode->ip = serverIP.ip();
    newNode->port = serverIP.port();
    cout << "Building new node at " << newNode->ip + ":" + to_string(newNode->port) << endl;
    // setup channel
    build_node_stub(newNode);
    // Send heartbeat and wait until it can receive requests before adding to list
    grpc::ClientContext hb_context;
    google::protobuf::Empty hb_request;
    google::protobuf::Empty hb_reply;
    int backoff = 1;
    // If you don't run heartbeat here, it seems to struggle intermittently after adding head and tail services
    while (true) {
        grpc::Status hb_status = newNode->stub->HeartBeat(&hb_context, hb_request, &hb_reply);
        if (!hb_status.ok()) {
            if (hb_status.error_code() == grpc::UNAVAILABLE ||
                    hb_status.error_code() == grpc::UNIMPLEMENTED) {
                cout << "...Attempting to connect with new node" << endl;
            } else {
                cout << "...Error: Something unexpected happened. Aborting registration" << endl;
                cout << hb_status.error_code() << ": " << hb_status.error_message()
                     << endl;
                return hb_status;
            }
        } else {
            cout << "...Was able to initiate communication" << endl;
            break;
        }
        sleep(backoff);
        backoff += 2;
    }

    // need to ID new state for old tail before changeMode
    server::State old_tail_state;
    if (tail == head) old_tail_state = server::HEAD;
    else old_tail_state = server::MIDDLE;

    /**
     * If new node is not first node then we need to bring volume up to speed
     */
    if (!master::nodeList.empty()){
        // set request params for current tail
        server::ChangeModeRequest cm_request;
        server::ServerIp * next_addr = cm_request.mutable_next_addr();
        next_addr->set_ip(newNode->ip);
        next_addr->set_port(newNode->port);
        cm_request.set_last_seq_num(request->last_seq_num());
        cm_request.set_new_state(old_tail_state);

        // context and reply
        server::ChangeModeReply cm_reply;
        grpc::ClientContext cm_context;

        // Update node it is no longer tail, it identifies what its
        // Current plan is to block here until new node is brought online
        // On changeMode old tail needs to initiate communication with new tail
        // and bring it up to date.  Nothing happens until this is complete
        backoff = 1;
        while (true){
            grpc::Status status = tail->stub->ChangeMode(&cm_context, cm_request, &cm_reply);
            if (!status.ok()) {
                if (status.error_code() == grpc::UNAVAILABLE) {
                    cout << "...upstream server unavailable" << endl;
                } else if (status.error_code() == grpc::UNIMPLEMENTED) {
                    cout << "...upstream server has not exposed calls" << endl;
                } else {
                    cout << "...Error: Something unexpected happened. Aborting registration" << endl;
                    cout << status.error_code() << ": " << status.error_message()
                         << endl;
                    return status;
                }
            } else {
                cout  << "...Tail has been notified of its new status" << endl;
                break;
            }
            sleep(backoff);
            backoff += 2;
        }

        // Set reply from master - adds IP of old tail
        master::ServerIp * reply_addr = reply->mutable_prev_addr();
        reply_addr->set_ip(master::tail->ip);
        reply_addr->set_port(master::tail->port);
    }

    /**
     * Lock node list, add new node, and update node states
     */
    // Push to nodeList - may want to extend lock, or wait until end to push to node list
    master::nodeList_mtx.lock();
        master::nodeList.push_back(newNode);
        if (master::nodeList.size() == 1){
            // Only node on list
            newNode->state = server::SINGLE;
            master::head = newNode;
        } else {
            // Update old tail state
            tail->state = old_tail_state;
            // Update tail position on master
            newNode->state = server::TAIL;
        }
        master::tail = newNode;
    master::nodeList_mtx.unlock();

    cout << "...Node registered" << endl;
    master::print_nodes();
    return grpc::Status::OK;
}