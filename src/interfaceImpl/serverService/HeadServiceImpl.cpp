#include <iostream>
#include "server.h"
#include "tables.hpp"
#include "storage.hpp"
#include "constants.hpp"
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
        
        cout << "Client called Write - next seq # is " << Tables::currentSeq << endl;
        //Add to replay log
        int addResult = Tables::replayLog.addToLog(request->clientrequestid());
        if (addResult < 0) {return grpc::Status::OK;}// means entry already exist in log or has been acked
        
//        cout << "Write checkpoint 1" << endl;
        
        Tables::PendingQueue::pendingQueueEntry entry;
        long seq = Tables::currentSeq++;
        
//        cout << "Write checkpoint 2" << endl;

        string m_data(request->data(), 0, Constants::BLOCK_SIZE);
//        cout << ":" << m_data << ": written to pending queue" << endl;
        entry.seqNum = seq;
        entry.volumeOffset = request->offset();
        entry.data = m_data;
//        entry.data = request->data();
        entry.reqId = request->clientrequestid();
        
//        cout << "Write checkpoint 3" << endl;
        
        Tables::pendingQueue.pushEntry(entry);
        
//        cout << "Write checkpoint 4" << endl;
        
        reply->set_seqnum(seq);
        
        cout << "...Finished Writing to pending queue ("
                << entry.reqId.ip() << ":"
                << entry.reqId.pid() << ":"
                << entry.reqId.timestamp().seconds() << ") to pending queue" <<  endl;
        
        return grpc::Status::OK;
}


/**+
 * Request to checksum chain
 *
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::HeadServiceImpl::ChecksumSystem (grpc::ServerContext *context,
                            const google::protobuf::Empty *request,
                            server::ChecksumReply *reply) {
    // Should just be able to call volume checksum and then send downstream
    // on return should verify results and return valid + (my CS == downstream CS)

    //first item, sent reply to true, will adjust as it comes back up chain
//    reply->set_valid(true);

    cout << "Running Checksum" << endl;
    server::ChecksumReply cs_request;
    cs_request.set_valid(true);
    if (cs_request.valid()) cout << "...initially valid" << endl;
    else cout << "...initially invalid" << endl;

    // Chain
    grpc::ClientContext cs_context;
    string my_cs, ds_cs;

    my_cs = Storage::checksum();
    if (server::state != server::TAIL && server::state != server::SINGLE){
        grpc::Status status = server::downstream->stub->ChecksumChain(&cs_context, cs_request, reply);
        if (!status.ok()){
            cout << "Something went terribly wrong with checksum" << endl;
        }
        ds_cs = reply->chk_sum();
    } else ds_cs = my_cs;

    cout << "...Checksum = " << my_cs << endl;
    reply->set_valid(cs_request.valid() && (ds_cs == my_cs));
    reply->set_chk_sum(my_cs);
    if (reply->valid()) cout << "...chain matches" << endl;
    else cout << "...chain inconsistent" << endl;

    return grpc::Status::OK;
}