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