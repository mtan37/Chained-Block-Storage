#include <iostream>
#include "server.h"
#include "storage.hpp"
#include <grpc++/grpc++.h>
#include "tables.hpp"
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
        if (hb_two) cout << "..(";
        else cout << ".(" ;
        cout  << server::get_state(server::state) << ")" << endl;
        hb_two = !hb_two;
        return grpc::Status::OK;
}

/**
 * Allows node to get message from server informing it that its state has changed
 * This can be caused by a new node being added, or by node failure
 * Two aspects to state change
 * a) New communicaiton lines, need to (re)build stub.  In certain cases some node-node
 *    communicaton needs to terminate (i.e writes headed downstream when our new state is tail)
 *    This can probably be handled in status check on write, like while(state!=TAIL) try again.
 *    This is true for relay acks as well when you become head.  Otherwise these actions can
 *    just retry and eventually the stub is updated.  Only concern is what happens if communication
 *
 * @param context
 * @param request
 * @param reply
 * @return
 */
grpc::Status server::MasterListenerImpl::ChangeMode (grpc::ServerContext *context,
    const server::ChangeModeRequest *request,
    server::ChangeModeReply *reply) {
    // Need to ensure we are not trying to register new node and change tail state at same time
    // TODO: Actually, is this an issue?  ChangeNode would be result of upstream failure
    // Would that interfere with initializing new server? Only upstream IP & stub would be changed
    // We likely will not be using TRANSITION anyways, so tail will stay tail or beceom SINGLE
    // which should be ok?
    changemode_mtx.lock();
    server::State old_state = server::state;
    server::state = server::TRANSITION;
    server::State new_state = static_cast<server::State>(request->new_state());
    cout << "Transitioning to " << server::get_state(new_state) << endl;

    // Identify upstream and downstream changes.  Each of these may impact writes\reply_acks
    if (request->has_prev_addr()) {
        server::upstream->ip =  request->prev_addr().ip();
        server::upstream->port =  request->prev_addr().port();
        server::build_node_stub(server::upstream);
    }
    if (request->has_next_addr()) {
        server::downstream->ip =  request->next_addr().ip();
        server::downstream->port =  request->next_addr().port();
        server::build_node_stub(server::downstream);
    }

    /*
     * If mode was tail, and new mode is not tail then need to bring new tail up to speed
     */
    if (new_state != server::TAIL && new_state != server::SINGLE
        && (old_state == server::TAIL || old_state == server::SINGLE)){
        //TODO: This is where we call function to deal with integrating new tail (i.e initialize new tail volume)
        //downstream->init_new_tail(request->last_seq_num());
        server::RestoreRequest restoreRequest;
        std::vector<std::pair<long, long>> restoreOffsets = Storage::get_modified_offsets(request->last_seq_num());
        for (auto i : restoreOffsets) {
            server::RestoreEntry *restoreEntry = restoreRequest.add_entry();
            restoreEntry->set_offset(i.first);
            restoreEntry->set_seqnum(i.second);
            char* buf = new char[4096];
            Storage::read(buf, i.first);
            restoreEntry->set_data(buf);
        }
        google::protobuf::Empty restoreReply;
        grpc::ClientContext restoreContext;
        downstream->stub->Restore(&restoreContext, restoreRequest, &restoreReply);
        cout << "...initialize new tail - last seq # was " << request->last_seq_num() << endl;
        
        //TODO: Forward replay log
        grpc::ClientContext replayContext;
        server::UpdateReplayLogRequest replayRequest;
        Tables::replayLog.getRelayLogContent(replayRequest);
        google::protobuf::Empty replayReply;
        downstream->stub->UpdateReplayLog(&replayContext, replayRequest, &replayReply);
    }

    if (new_state != old_state) {
        bool clear_upstream = false, clear_downstream = false;
        switch (new_state) {
            case (server::SINGLE):
                clear_upstream = true;
                clear_downstream = true;
                if (old_state != server::HEAD) server::launch_head();
                if (old_state != server::TAIL) server::launch_tail();
                break;
            case (server::HEAD):
                if (old_state == server::SINGLE) server::kill_tail();
                if (old_state != server::SINGLE) server::launch_head();
                clear_upstream = true;
                break;
            case (server::TAIL):
                if (old_state != server::SINGLE) server::launch_tail();
                clear_downstream = true;
                break;
            case (server::MIDDLE):
                if (old_state == server::TAIL) server::kill_tail();
                break;
            default:
                break;
        }
        if (clear_upstream) {
            server::upstream->ip = "";
            server::upstream->port = -1;
            server::upstream->stub = nullptr;
        }
        if (clear_downstream) {
            server::downstream->ip = "";
            server::downstream->port = -1;
            server::downstream->stub = nullptr;
        }
    } else {
        // state is the same, mid-failure, need to return sequence number
        if (request->has_prev_addr()){ // we are m+1 in mid failure
            // TODO: Should we have lock around here so we don't start sending commits back
            // to m-1 before we are able to deal with failure resolution?
            // on the sent list.  This would correspond to the last seq # written.  If we are here
            // then we are in mid failure, so writes should be up to date. Might not actually
            // be an issue for commits.  Only going to remove items form m-1 sent list that we
            // have already seen
            reply->set_lastreceivedseqnum(Tables::writeSeq);
            cout << "I am m+1 in mid failure, returning my sequence number" << endl;
            cout << "my upstream addy is " <<  server::upstream->ip << ":" << server::upstream->port;
        }
        // TODO: Below is where we send m+1 updated sent list
        // Same as m-4 We do not want this node to be sending things downstream while we do this? if IP
        // is updated above then this will be happening. This will add writes to our sent list
        // which will then get resent below
        // So we need to to lock downstream->relay_write in relay_write_background.  Since we
        // are sending things downstream, I think it needs to be the lock
        if (request->has_next_addr()){ // we are m-1 in mid failure
            // TODO: Here is were we deal with updating m+1
            cout << "I am m-1 in mid failure, need to update m+1" << endl;
            cout << "my upstream addy is " <<  server::downstream->ip << ":" << server::downstream->port << endl;
            // Need to generate list of writes and send it to m+1 based on sequence and sent list
            long current_resend_seq = request->last_seq_num()+1;
            std::list<Tables::SentList::sentListEntry> resend;
            try{
                // TODO Fix this
//                resend = Tables::sentList.getSentListRange(current_resend_seq);
                //resend = Tables::sentList.popSentListRange(current_resend_seq);
            } catch (std::length_error){
                cout << "...No items to update" << endl;
            }

            // need to get data from volume
            // Should be able to send using node->RelayWrite();
            std::list<Tables::SentList::sentListEntry>::iterator it;
            for (it = resend.begin(); it != resend.end(); it++){
                cout << "...iterating through resend list" << endl;
                Tables::SentList::sentListEntry current = *it;
                string data = "";
                if (Storage::read_sequence_number(data, current_resend_seq, current.volumeOffset)){
                    // Found in volume, should be good to go
                    grpc::ClientContext context;
                    server::RelayWriteRequest request;
                    // setup request
                    request.set_data(data);
                    request.set_offset(current.volumeOffset);
                    request.set_seqnum(current_resend_seq);
                    // Copy sent list client ID over to request
                    server::ClientRequestId *clientRequestId = request.mutable_clientrequestid();
                    clientRequestId->set_ip(current.reqId.ip());
                    clientRequestId->set_pid(current.reqId.pid());
                    google::protobuf::Timestamp timestamp = *clientRequestId->mutable_timestamp();
                    timestamp = current.reqId.timestamp();
                    // hold reply
                    google::protobuf::Empty RelayWriteReply;
                    // send downstream
                    while(true){
                        grpc::Status status = server::downstream->stub->RelayWrite(&context, request, &RelayWriteReply);
                        if (!status.ok()) {
                            cout << "...Error: Unable to send sent list item to m+1, trying again " <<  endl;
                            cout << status.error_code() << ": " << status.error_message()
                                 << endl;
                        } else {
                            cout << "...Sent " << current_resend_seq << endl;
                            break;
                        }
                    }
                } else {
                    // Not sure what we would do here
                    cout << "Unexpected error occurred while dealing with downstream mid failure" << endl;
                }
                current_resend_seq++;
            }
        }
    }



    server::state = new_state;

    cout << "...New state = " << server::get_state(server::state) << endl;
    if (server::upstream)
    cout << "...New upstream IP = " << server::upstream->ip << endl;
    if (server::downstream)
    cout << "...New downstream IP = " << server::downstream->ip << endl;
    changemode_mtx.unlock();
    return grpc::Status::OK;
}