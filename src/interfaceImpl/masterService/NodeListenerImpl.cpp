#include <iostream>
#include "master.h"
#include "server.h"
#include <grpc++/grpc++.h>

using namespace std;

/**
 * Nodes need to register with server to get added to the chain. When it regestires the
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
    
        master::Node *newNode = new Node;
        master::ServerIp serverIP = request->server_ip();
        newNode->ip = serverIP.ip();
        newNode->port = serverIP.port();

        // setup channel
        string node_addr(newNode->ip + ":" + to_string(newNode->port));
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
        newNode->stub = server::MasterListener::NewStub(channel);
        // Push to nodeList;
        master::nodeList.push_back(newNode);

        // If first server
        if (master::nodeList.size() == 1){
            cout << "Cluster initializing" << endl;
            // Only node on list
            master::head = newNode;
            master::tail = newNode;
        } else {
            // Set reply from master
            master::ServerIp * reply_addr = reply->mutable_prev_addr();
            reply_addr->set_ip(master::tail->ip);
            reply_addr->set_port(master::tail->port);

            // Send current tail change_node
            grpc::ClientContext cm_context;
            // set reqeust params
            server::ChangeModeRequest cm_request;
            server::ServerIp * next_addr = cm_request.mutable_next_addr();
            next_addr->set_ip(newNode->ip);
            next_addr->set_port(newNode->port);

            server::ChangeModeReply cm_reply;
            //TODO: Handle sequence numbers

            grpc::Status status = tail->stub->ChangeMode(&cm_context, cm_request, &cm_reply);

            if (!status.ok()) {
                cout << "Failed to relay write ack to next node" << endl;
                //TODO: Retry?
            } else {
                cout  << "Sent ChangeMode to tail" << endl;
            }

        }
        
        return grpc::Status::OK;
}