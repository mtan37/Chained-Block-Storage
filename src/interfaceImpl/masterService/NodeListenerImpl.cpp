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
        // Create node
        master::Node *newNode = new Node;
        master::ServerIp serverIP = request->server_ip();
        newNode->ip = serverIP.ip();
        newNode->port = serverIP.port();
        cout << "Building new node at " << newNode->ip + ":" + to_string(newNode->port) << endl;
        // setup channel
        string node_addr(newNode->ip + ":" + to_string(newNode->port));
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
        newNode->stub = server::MasterListener::NewStub(channel);
        // Push to nodeList
        master::nodeList_mtx.lock();
        master::nodeList.push_back(newNode);
        master::nodeList_mtx.unlock();
        // If first server setup as single server
        if (master::nodeList.size() == 1){
            // Only node on list
            newNode->state = server::SINGLE;
            master::head = newNode;
            master::tail = newNode;
        } else { // Extend chain
            // Set reply from master
            master::ServerIp * reply_addr = reply->mutable_prev_addr();
            reply_addr->set_ip(master::tail->ip);
            reply_addr->set_port(master::tail->port);
            // set reqeust params
            server::ChangeModeRequest cm_request;
            server::ServerIp * next_addr = cm_request.mutable_next_addr();
            next_addr->set_ip(newNode->ip);
            next_addr->set_port(newNode->port);
            server::ChangeModeReply cm_reply;
            grpc::ClientContext cm_context;
            //TODO: Handle sequence numbers

            // Update old tail state
            if (tail == head) tail->state = server::HEAD;
            else tail->state = server::MID;
            // Update node it is no longer tail, it identifies what its
            // Current plan is to block here until new node is brought online
            // On changeMode old tail needs to initiate communication with new tail
            // and bring it up to date.  Nothing happens until this is complete
            grpc::Status status = tail->stub->ChangeMode(&cm_context, cm_request, &cm_reply);
            if (!status.ok()) {
                cout << "Error: Failed to relay write ack to next node" << endl;
                //TODO: Retry? How do we handle this?
            } else {
                cout  << "...Tail has been notified of its new status" << endl;
            }
            // Update tail position on master
            master::tail = newNode;
        }
        return grpc::Status::OK;
}