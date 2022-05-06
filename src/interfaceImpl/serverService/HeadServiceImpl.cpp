#include <iostream>
#include "server.h"
#include "tables.hpp"

using namespace std;


/**
 * Get write from client, add to pending queue, update current seq
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::HeadServiceImpl::Write (
    grpc::ServerContext *context,
    const server::WriteRequest *request,
    server::WriteReply *reply) {
        

        //Add to replay log
        int addResult = Tables::replayLog.addToLog(request->clientrequestid());
        if (addResult < 0) {return grpc::Status::OK;}// means entry already exist in log or has been acked
        
        Tables::PendingQueue::pendingQueueEntry entry;
        entry.seqNum = Tables::currentSeq;
        entry.volumeOffset = request->offset();
        entry.data = request->data();
        
        Tables::pendingQueue.pushEntry(entry);
        
        reply->set_seqnum(Tables::currentSeq);
        Tables::currentSeq++;
        
        return grpc::Status::OK;
}