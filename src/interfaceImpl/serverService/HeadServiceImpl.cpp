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
        
        cout << "Called Write" << endl;
        //Add to replay log
        int addResult = Tables::replayLog.addToLog(request->clientrequestid());
        if (addResult < 0) {return grpc::Status::OK;}// means entry already exist in log or has been acked
        
        cout << "Write checkpoint 1" << endl;
        
        Tables::PendingQueue::pendingQueueEntry entry;
        long seq = Tables::currentSeq++;
        
        cout << "Write checkpoint 2" << endl;
        
        entry.seqNum = seq;
        entry.volumeOffset = request->offset();
        entry.data = request->data();
        
        cout << "Write checkpoint 3" << endl;
        
        Tables::pendingQueue.pushEntry(entry);
        
        cout << "Write checkpoint 4" << endl;
        
        reply->set_seqnum(seq);
        
        cout << "Finished Write" << endl;
        
        return grpc::Status::OK;
}