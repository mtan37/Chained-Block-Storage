#include <iostream>
#include "server.h"
#include <grpc++/grpc++.h>

using namespace std;

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
        return grpc::Status::OK;
}

/**
 * Allows node to get message from server informing it that its state has changed
 *
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::MasterListenerImpl::ChangeMode (grpc::ServerContext *context,
    const server::ChangeModeRequest *request,
    server::ChangeModeReply *reply) {
        
        //TODO: Set prev/next node info (don't replace if empty request, case with new tail), change HEAD/MIDDLE/TAIL state
        cout << "Im a changeMode" << endl;

//        if (request->has_prevaddr()) {
//            server::ServerIp prevNode = request->prevaddr();
//            server::prev_node_ip = prevNode.ip();
//            server::prev_node_port = prevNode.port();
//        }
//        if (request->has_nextaddr()) {
//            server::ServerIp nextNode = request->nextaddr();
//            server::next_node_ip = nextNode.ip();
//            server::next_node_port = nextNode.port();
//        }
//        if (request->has_prevaddr() && !request->has_nextaddr()) server::state = server::TAIL;
//        if (!request->has_prevaddr() && request->has_nextaddr()) server::state = server::HEAD;
//        if (!request->has_prevaddr() && !request->has_nextaddr()) server::state = server::SINGLE;
//        if (request->has_prevaddr() && request->has_nextaddr()) server::state = server::MIDDLE;
//
//        //TODO: Handle sequence numbers and reply
        
        return grpc::Status::OK;
}