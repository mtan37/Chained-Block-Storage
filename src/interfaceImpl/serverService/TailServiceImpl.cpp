#include <iostream>
#include "server.h"
#include "tables.hpp"
#include "storage.hpp"

using namespace std;


grpc::Status server::TailServiceImpl::WriteAck (
    grpc::ServerContext *context,
    const server::WriteAckRequest *request,
    server::WriteAckReply *reply) {
    
        int result = Tables::replayLog.ackLogEntry(request->clientrequestid());
        cout<< "ackLogEntry result is " << result << endl;
        if (result == 0)
            reply->set_committed(true);
        else
            reply->set_committed(false);
            
        while (server::state == server::TAIL || server::state == server::SINGLE) {
            grpc::ClientContext relay_context;
            google::protobuf::Empty ackReplayLogReply;
            server::AckReplayLogRequest ackReplayLogRequest;
            server::ClientRequestId *clientRequestId = ackReplayLogRequest.mutable_clientrequestid();
            clientRequestId->set_ip(request->clientrequestid().ip());
            clientRequestId->set_pid(request->clientrequestid().pid());
            google::protobuf::Timestamp *timestamp = clientRequestId->mutable_timestamp();
            timestamp->set_seconds(request->clientrequestid().timestamp().seconds());
            timestamp->set_nanos(request->clientrequestid().timestamp().nanos());

            grpc::Status status = server::upstream->stub->AckReplayLog(&relay_context, ackReplayLogRequest, &ackReplayLogReply);
            cout << "...ReplayLog forward attempt to " << server::upstream->ip << ":"
                << server::upstream->port << " returned: " << status.error_code() << endl;
            if (status.ok()) break;
            server::build_node_stub(server::upstream);
        
        }
        
        return grpc::Status::OK;
}

grpc::Status server::TailServiceImpl::Read (
    grpc::ServerContext *context,
    const server::ReadRequest *request,
    server::ReadReply *reply) {
        char* buf = new char[4096];
        Storage::read(buf, request->offset());
        reply->set_data(buf);
        
        return grpc::Status::OK;
}