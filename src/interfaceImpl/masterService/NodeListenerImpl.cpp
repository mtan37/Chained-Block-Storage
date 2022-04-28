#include <iostream>
#include "master.h"
#include "server.h"
#include <grpc++/grpc++.h>

using namespace std;


grpc::Status master::NodeListenerImpl::Register (
    grpc::ServerContext *context,
    const master::RegisterRequest *request,
    master::RegisterReply *reply) {
    
        master::Node newNode;
        master::ServerIp serverIp = request->serverip();
        newNode.ip = serverIp.ip();
        newNode.port = serverIp.port();
        master::nodeList.push_back(newNode);
        
        master::ServerIp tail;
        master::Node tailNode = master::nodeList.back();
        tail.set_ip(tailNode.ip);
        tail.set_port(stoi(tailNode.port));
        
        reply->set_prevaddr(tail);
        
        string tail_addr = tailNode.ip + ":" + tailNode.port;
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(tail_addr, grpc::InsecureChannelCredentials(), args);
        std::unique_ptr<server::MasterListener::Stub> tail_stub = server::MasterListener::NewStub(channel);
        grpc::ClientContext changemode_context;
        server::ChangeModeReply ChangeModeReply;
        
        server::ChangeModeRequest changemode_request;
        changemode_request.set_nextaddr(serverIp);
        //TODO: Handle sequence numbers
        
        grpc::Status status = tail_stub->ChangeMode(&changemode_context, changemode_request, &ChangeModeReply);

        if (!status.ok()) {
            cout << "Failed to relay write ack to next node" << endl;
            //TODO: Retry?
        }
        
        return grpc::Status::OK;
}