#include <iostream>
#include "server.h"
#include "tables.hpp"
#include "storage.hpp"
#include "constants.hpp"

#include <grpc++/grpc++.h>

// the grpc interface imports
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"

using namespace std;

grpc::Status server::NodeListenerImpl::RelayWrite (grpc::ServerContext *context,
    const server::RelayWriteRequest *request,
    google::protobuf::Empty *reply) {
        cout << "Upstream called RelayWrite - sent seq # " << request->seqnum() << endl;
        //Add to replay log
        cout << "...Adding to replay log" << endl;
        int addResult = Tables::replayLog.addToLog(request->clientrequestid());
        cout << "...Printing replay log" << endl;

        if (addResult < 0) {return grpc::Status::OK;}// means entry already exist in log or has been acked
        
        Tables::PendingQueue::pendingQueueEntry entry;
        entry.seqNum = request->seqnum();
        entry.volumeOffset = request->offset();
        string m_data(request->data(), 0, Constants::BLOCK_SIZE);
//        entry.data = request->data();
        entry.data = m_data;
        entry.reqId = request->clientrequestid();

        
        cout << "...Added entry " << request->seqnum() << " to pendingqueue with reqId " << entry.reqId.ip() << ":" << entry.reqId.pid() << ":" << entry.reqId.timestamp().seconds() << endl;
        
        Tables::pendingQueue.pushEntry(entry);

        cout << "...Finished Writing ("
             << entry.reqId.ip() << ":"
             << entry.reqId.pid() << ":"
             << entry.reqId.timestamp().seconds() << ") to pending queue" <<  endl;

        return grpc::Status::OK;
}

/**
 * Pops seq# off sent list and Commits based on seq # sent from downstream node.
 *
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::NodeListenerImpl::RelayWriteAck (grpc::ServerContext *context,
    const server::RelayWriteAckRequest *request,
    google::protobuf::Empty *reply) {
        cout << "RelayWriteAck called" << endl;
        while (Tables::commitSeq + 1 != request->seqnum()) {}
        //Remove from sent list
        Tables::SentList::sentListEntry sentListEntry = Tables::sentList.popEntry((int) request->seqnum());
        cout << "...committing to storage" << endl;
        Storage::commit(request->seqnum(), sentListEntry.volumeOffset);
        cout << "...committing log entry on replay log" << endl;
        int result = Tables::replayLog.commitLogEntry(sentListEntry.reqId);

        cout << "...Committed entry " << request->seqnum() << " with reqId "
            << sentListEntry.reqId.ip() << ":"
            << sentListEntry.reqId.pid() << ":"
            << sentListEntry.reqId.timestamp().seconds() << " with result " << result << endl;
        cout << "...Printing replay log" << endl;
           
        Tables::commitSeq = (int) request->seqnum();
        
        cout << "...RelayWriteAck checkpoint 1" << endl;
        
        while (server::state != HEAD && server::state != SINGLE) {
            //Relay to previous nodes
            grpc::ClientContext relay_context;
            google::protobuf::Empty RelayWriteAckReply;
            
            grpc::Status status = server::upstream->stub->RelayWriteAck(&relay_context, *request, &RelayWriteAckReply);
            cout << "...Forward attempt to " << server::upstream->ip << ":"
                 << server::upstream->port << " returned: " << status.error_code() << endl;
            if (status.ok()) break;
            sleep(1);
        }
        cout << "...RelayWriteAck checkpoint 2" << endl;
        return grpc::Status::OK;
}


/**
 * TODO : What is this for, nothing is calling it?
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::NodeListenerImpl::ReplayLogChange (grpc::ServerContext *context,
        const server::ReplayLogChangeRequest *request,
        google::protobuf::Empty *reply) {
        // TODO 
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::Restore (grpc::ServerContext *context,
        const server::RestoreRequest *request,
        google::protobuf::Empty *reply) {

        cout << "...Running restore" << endl;
        long maxseq = Storage::get_sequence_number();
        for (int i = 0; i < request->entry_size(); i++) {
            server::RestoreEntry entry = request->entry(i);
//            cout << "...(" << i << ") restoring seq # "  << entry.seqnum() << endl;
            Storage::write(entry.data(), entry.offset(), entry.seqnum());        
            Storage::commit(entry.seqnum(), entry.offset());    
            if (entry.seqnum() > maxseq) maxseq = entry.seqnum();
        }
        Tables::writeSeq = maxseq;
        Tables::commitSeq = maxseq;
        Tables::currentSeq = maxseq + 1;
        cout << "...Finished restore" << endl;
        return grpc::Status::OK;       
}

grpc::Status server::NodeListenerImpl::UpdateReplayLog (grpc::ServerContext *context,
        const server::UpdateReplayLogRequest *request,
        google::protobuf::Empty *reply) {
        cout << "...Running UpdateReplayLog" << endl;
        Tables::replayLog.initRelayLogContent(request);
        cout << "...Finished UpdateReplayLog" << endl;
        return grpc::Status::OK;
}

grpc::Status server::NodeListenerImpl::AckReplayLog (grpc::ServerContext *context,
        const server::AckReplayLogRequest *request,
        google::protobuf::Empty *reply) {
        int result = Tables::replayLog.ackLogEntry(request->clientrequestid());
        cout<< "(URL) ackLogEntry result is " << result << endl;
        while (server::state != server::HEAD && server::state != server::SINGLE) {
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
            cout << "...(URL) ReplayLog forward attempt to " << server::upstream->ip << ":"
                 << server::upstream->port << " returned: " << status.error_code() << endl;
            if (status.ok()) break;
            server::build_node_stub(server::upstream);

        }
        cout << "(URL) exiting" << endl;
        return grpc::Status::OK;        
}

/**
 * Checksum volume, if not tail send downstream
 * if sent downstream then need to check that our checksum matches and return
 * valid + (my CS == downstream CS)
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::NodeListenerImpl::ChecksumChain (grpc::ServerContext *context,
                            const server::ChecksumReply *request,
                            server::ChecksumReply *reply){
    // Chain
    cout << "Running Checksum" << endl;
    grpc::ClientContext cs_context;
    if (request->valid()) cout << "...initially valid" << endl;
    else cout << "...initially invalid" << endl;

    string my_cs, ds_cs;

    my_cs = Storage::checksum();
    if (server::state != server::TAIL && server::state != server::SINGLE){
        grpc::Status status = server::downstream->stub->ChecksumChain(&cs_context, *request, reply);
        if (!status.ok()){
            cout << "Something went terribly wrong with checksum" << endl;
        }
        ds_cs = reply->chk_sum();
    } else ds_cs = my_cs;

    cout << "Checksum = " << my_cs << endl;
    reply->set_valid(request->valid() && (ds_cs == my_cs));
    reply->set_chk_sum(my_cs);
    if (reply->valid()) cout << "...chain matches" << endl;
    else cout << "...chain inconsistent" << endl;
    return grpc::Status::OK;
}
