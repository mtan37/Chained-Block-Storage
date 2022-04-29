#include <iostream>
#include "server.h"
#include <grpc++/grpc++.h>

using namespace std;
bool hb_two = false;
/**
 * Responds to master request for heartbeat.  Informs master that this node is still alive
 * @param context
 * @param request
 * @param reply
 * @return status OK
 */
grpc::Status server::MasterListenerImpl::HeartBeat (grpc::ServerContext *context,
    const google::protobuf::Empty *request,
    google::protobuf::Empty *reply){
        // Should not have to do anything but reply.
        if (hb_two) cout << ".." << endl;
        else cout << "." << endl;
        hb_two = !hb_two;
        return grpc::Status::OK;
}

/**
 * Allows node to get message from server informing it that its state has changed
 * This can be caused by a new node being added, or by node failure
 *
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
 *
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::MasterListenerImpl::ChangeMode (grpc::ServerContext *context,
    const server::ChangeModeRequest *request,
    server::ChangeModeReply *reply) {

        cout << "Transitioning to new state." << endl;
        server::state = server::TRANSITION;
        //TODO: Set prev/next node info (don't replace if empty request, case with new tail), change HEAD/MIDDLE/TAIL state


        if (request->has_prev_addr()) {
            server::upstream.ip =  request->prev_addr().ip();
            server::upstream.port =  request->prev_addr().port();
            // build communication channel
            string node_addr(server::upstream.ip + ":" + to_string(server::upstream.port));
            grpc::ChannelArguments args;
            args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
            std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
            server::upstream.stub = server::NodeListener::NewStub(channel);
        }
        if (request->has_next_addr()) {
            server::downstream.ip =  request->next_addr().ip();
            server::downstream.port =  request->next_addr().port();
            // Build communication channel
            string node_addr(server::downstream.ip + ":" + to_string(server::downstream.port));
            grpc::ChannelArguments args;
            args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
            std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
            server::downstream.stub = server::NodeListener::NewStub(channel);
        }

        // We may need additional info for failure scenarios
        if (server::upstream.ip != "" && server::downstream.ip == "") server::state = server::TAIL;
        if (server::upstream.ip == "" && server::downstream.ip != "") server::state = server::HEAD;
        if (server::upstream.ip != "" && server::downstream.ip != "") server::state = server::MIDDLE;
        if (server::upstream.ip == "" && server::downstream.ip == "") server::state = server::SINGLE;


        cout << "...New state = " << server::get_state() << endl;

//        //TODO: Handle sequence numbers and reply
        
        return grpc::Status::OK;
}