#include <iostream>
#include "server.h"
#include "storage.hpp"
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
        cout << "...initialize new tail - last seq # was " << request->last_seq_num() << endl;
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
            // TODO: What sequence number do we send here.  Should be highest sequenec number
            // this node has seen, but we don't seem to track this anywhere.  
            reply->set_lastreceivedseqnum(0);
            cout << "I am m+1 in mid failure, returning my sequence number" << endl;
            cout << "my upstream addy is " <<  server::upstream->ip << ":" << server::upstream->port;
        }
        if (request->has_next_addr()){ // we are m-1 in mid failure
            // TODO: Here is were we deal with updating m+1
            cout << "I am m-1 in mid failure, need to update m+1" << endl;
            cout << "my upstream addy is " <<  server::downstream->ip << ":" << server::downstream->port;
            // Need to generate list of writes and send it to m+1 based on sequence and sent list
            std::list<SentList::sentListEntry> resend = popSentListRange(request->lastreceivedseqnum());
            // need to get data from volume
            // Should be able to send using node->RelayWrite();
            std::list<sentListEntry>::iterator it;
            while(it != this->list.end()){

            }
        }
    }



    server::state = new_state;

    cout << "...New state = " << server::get_state(server::state) << endl;
    changemode_mtx.unlock();
    return grpc::Status::OK;
}