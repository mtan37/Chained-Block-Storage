#include <iostream>

#include <grpc++/grpc++.h>
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"
#include <google/protobuf/empty.pb.h>

#include "constants.hpp"
#include "tables.hpp"
#include "storage.hpp" 
#include "master.h"
#include "server.h"
#include "helper.h"

#include <thread>

using namespace std;

// Global variables
string def_volume_name = "volume";
string master_ip = "";

int my_port = Constants::SERVER_PORT;
bool start_clean = false;

namespace Tables {
    std::atomic<long> currentSeq(0);
    long writeSeq;
    long commitSeq;
};

namespace server {
    server::Node *downstream;
    server::Node *upstream;
    std::mutex changemode_mtx;
    State state;
    std::unique_ptr<grpc::Server> headService;
    std::unique_ptr<grpc::Server> tailService;
    std::string my_ip = "0.0.0.0";

    string get_state(State state) {
        switch (state) {
            case server::HEAD:
                return "Head";
                break;
            case server::TAIL:
                return "Tail";
                break;
            case server::MIDDLE:
                return "Mid";
                break;
            case server::SINGLE:
                return "Single Server";
                break;
            case server::INITIALIZE:
                return "Initializing";
                break;
            case server::TRANSITION:
                return "Transitioning";
                break;
        }
        return "";
    };

    // Run grpc service in a loop
    void run_service(grpc::Server *server, std::string serviceName) {
        std::cout << "Starting to " << serviceName << "\n";
        std::cout << "Checking head service " <<  headService.get() << endl;
        server->Wait();
        cout << "Will no longer " << serviceName << endl;
    }

    /*
     * Launch tail service.  Service can be launched multiple times per server based on
     * failure scenarios, and must be shutdown appropriately when no longer tail.
     */
    void launch_tail(){
        std::string my_address(server::my_ip + ":" + to_string(my_port+Constants::tail_port));
        server::TailServiceImpl *tailServiceImpl = new server::TailServiceImpl();
        grpc::ServerBuilder tailBuilder;
        tailBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
        tailBuilder.RegisterService(tailServiceImpl);
        // Thread server out and start listening
        server::tailService = tailBuilder.BuildAndStart();
        std::thread (server::run_service, tailService.get(), "listen as tail").detach();
        std::thread (relay_write_ack_background).detach();
    }

    // We need to kill tail when promoted to mid\head.  But keep in mind that
    // We can become tail again if current tail fails
    void kill_tail(){
        server::tailService->Shutdown();
    }

    /**
     * Launch head.  Head never reverts, so we can detach head service thread.
     */
    void launch_head(){
        std::string my_address(server::my_ip + ":" + to_string(my_port + Constants::head_port));
        cout << "About to launch head at " << my_address << endl;
        server::HeadServiceImpl *headServiceImpl = new server::HeadServiceImpl();
        grpc::ServerBuilder headBuilder;
        headBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
        headBuilder.RegisterService(headServiceImpl);
        // Thread server out and start listening
        server::headService = headBuilder.BuildAndStart();
        std::thread (server::run_service, server::headService.get(), "listen as head").detach();
    }

    // Generic method to build up and downstream stubs
    void build_node_stub(server::Node* node){
        string node_addr(node->ip + ":" + to_string(node->port));
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
        node->stub = server::NodeListener::NewStub(channel);
    }
};


/**
 * Server will come up either in single node state or as intializing then tail
 * Not totally sure how this is going to work yet
 * @return
 */
int register_server() {
    // Set up our state variables
    server::downstream = new server::Node;
    server::upstream = new server::Node;
    // Get setup to call master->register
    // Set up our request
    master::RegisterRequest request;
    master::ServerIp * serverIP = request.mutable_server_ip();
    serverIP->set_ip(server::my_ip);
    serverIP->set_port(my_port);
    // add last seq # to request
    request.set_last_seq_num(Tables::commitSeq);
//    cout << "My last sequence number was " << request.last_seq_num() << endl;
    // Create container for reply
    master::RegisterReply reply;
    // Register with master server
    string master_address = master_ip + ":" + Constants::MASTER_PORT;
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
    int backoff = 1;
    while (true){
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(master_address, grpc::InsecureChannelCredentials(), args);
        std::unique_ptr<master::NodeListener::Stub> master_stub = master::NodeListener::NewStub(channel);
        grpc::ClientContext context;
        grpc::Status status = master_stub->Register(&context, request, &reply);
        // Check return status on register call
        // Once we get return we are ready to go.  Have been brought up to speed by previous
        // tail, or have been brought up as single server and act as source of truth
        if (!status.ok()) {
            cout << "Can't contact master at " << master_address << endl;
        } else {
            cout << "Was able to contact master" << endl;
            break;
        }
        sleep(backoff);
        backoff += 1;
    }
    // Set state, and if present build communication with old tail
    if (reply.has_prev_addr()) {
        master::ServerIp prev_addy_ip = reply.prev_addr();
        server::upstream->ip =  prev_addy_ip.ip();
        server::upstream->port =  prev_addy_ip.port();
        //build channel stub to upstream
        server::build_node_stub(server::upstream);
//        string node_addr(server::upstream->ip + ":" + to_string(server::upstream->port));
//        grpc::ChannelArguments args;
//        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
//        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
//        server::upstream->stub = server::NodeListener::NewStub(channel);
        //TODO: Test this connection works?
        server::state = server::TAIL;
    } else { // Starting in single server mode
        server::state = server::SINGLE;
        // Need to launch head service here
        server::launch_head();
    }

    // Launch tail service here - could potentially launch in main() since all nodes come up as tail
    server::launch_tail();
    cout << "My state is " << server::get_state(server::state) << endl;
    return 0;
}

void relay_write_background() {
    while(true) {
        if ( Tables::pendingQueue.getQueueSize() ) {
            cout << "(RWB) WriteSeq is " << Tables::writeSeq << " and seqNum on pending list is "
                << Tables::pendingQueue.peekEntry().seqNum << endl;
            while (Tables::pendingQueue.peekEntry().seqNum != Tables::writeSeq + 1) {}
            Tables::PendingQueue::pendingQueueEntry pending_entry = Tables::pendingQueue.popEntry();
            cout << "...(RWB) Pulling entry " << pending_entry.seqNum << " off the pending list ("
                    << pending_entry.reqId.ip() << ":"
                    << pending_entry.reqId.pid() << ":"
                    << pending_entry.reqId.timestamp().seconds() << ")" << endl;
            while (server::state != server::TAIL && server::state != server::SINGLE) { 
                cout << "...(RWB) Relaying write downstream" << endl;
                grpc::ClientContext context;
                server::RelayWriteRequest request;

                request.set_data(pending_entry.data);
                request.set_offset(pending_entry.volumeOffset);
                request.set_seqnum(pending_entry.seqNum);
                google::protobuf::Empty RelayWriteReply;
                server::ClientRequestId *clientRequestId = request.mutable_clientrequestid();
                clientRequestId->set_ip(pending_entry.reqId.ip());
                clientRequestId->set_pid(pending_entry.reqId.pid());
                google::protobuf::Timestamp *timestamp = clientRequestId->mutable_timestamp();
                timestamp->set_seconds(pending_entry.reqId.timestamp().seconds());
                timestamp->set_nanos(pending_entry.reqId.timestamp().nanos());

                string node_addr(server::downstream->ip + ":" + to_string(server::downstream->port));
                grpc::ChannelArguments args;
                args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
                std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
                auto stub = server::NodeListener::NewStub(channel);


                grpc::Status status = stub->RelayWrite(&context, request, &RelayWriteReply);
                cout << "......(RWB) Forward attempt to " << server::downstream->ip << ":" << server::downstream->port << ": " << status.error_code() << endl;
                cout << "......(RWB) ReqID was " << pending_entry.reqId.ip() << ":" << pending_entry.reqId.pid() << ":" << pending_entry.reqId.timestamp().seconds() << endl;
                cout << "......(RWB) Checking request ReqID: " << request.clientrequestid().timestamp().seconds() << endl;
                if (status.ok()) break;
                sleep(1);
                
            } //End forwarding if non-tail

            cout << "...(RWB) Background write thread checkpoint 1 (writing to volume)" << endl;
            cout << "...(RWB) WRITING Seq # " << pending_entry.seqNum << " - offset (" << pending_entry.volumeOffset   << ")" << endl;
            cout << "...(RWB) WRITING :" << pending_entry.data.length() << ":" << endl;
            //Need clarification on file vs volume offset
            // TODO: Hypothetical lock
            string m_data(pending_entry.data, 0, Constants::BLOCK_SIZE);
            Storage::write(m_data, pending_entry.volumeOffset, pending_entry.seqNum);
            cout << "...(RWB) Background write thread checkpoint 2 (written to volume)" << endl;
            Tables::writeSeq++;
            
            //Add to sent list
            // TODO: With current approach we don't want to add to sent list if tail, but with this layout
            // we could run into issues if tail state changes while work is in progress both with this
            // and with committing below, although this is unclear without knowing how we are doing
            // node integration.  If it needs ops to stop while it builds the list to send downstream
            // then we need to stop committing until our state changes to T-1. We only ever stop being
            // tail if a new node is added, at which point our state changes to transition and the new
            // tail will be brought up to speed.  New writes that come in while this happens need to just
            // stall until we have a resolution.  But writes in progress could be problematic.
            // I think the best approach would be to have a lock about here and either add things
            // to the sent list, or run commit.  When we start to register a new node we need to obtain
            // that lock, so below either finishes or does not start until the process is complete
            //
            // Also keep in mind that in addition to sending over items from volume, we need to forward
            // sent list to new tail
            //
            // An alternative approach would be to add things to sent list either way, and remove
            // commit code here, then have thread that launches when we launch tail services
            // that takes things off of sent list and commits them.  We would still need the lock
            // though.  This approach would stop stack from building up while we are adding new node
            // but we might not wan

            Tables::SentList::sentListEntry sent_entry;
            sent_entry.volumeOffset = pending_entry.volumeOffset;
            sent_entry.reqId = pending_entry.reqId;
            Tables::sentList.pushEntry(pending_entry.seqNum, sent_entry);

            cout << "(RWB) Wrote entry, added to sentList" << endl;
        }
    }
}

/**
 * Pulls items of sent list sequentially and commits them.  Relays commit
 * upstream if tail
 */
void relay_write_ack_background() {
    cout << "(RWAB) STARTING RELAY WRITE ACK BACKGROUND" << endl;
    while (server::state == server::TAIL || server::state == server::SINGLE) { 
        Tables::SentList::sentListEntry sentListEntry;
        //Remove from sent list
        try {
            long seq = Tables::commitSeq + 1;
            sentListEntry = Tables::sentList.popEntry((int) seq);
            //Will throw exception here if seq is not sequentially next
            cout << "...(RWAB)Printing replay log" << endl;
            Tables::replayLog.printRelayLogContent();
            cout << "...(RWAB) Pulling entry " << seq << " off the sent list" << endl;
            cout << "...(RWAB) committing to storage" << endl;
            Storage::commit(seq, sentListEntry.volumeOffset);
            cout << "...(RWAB) committing log entry on replay log" << endl;
            int result = Tables::replayLog.commitLogEntry(sentListEntry.reqId);
                
            Tables::commitSeq++;
            cout << "...(RWAB) Committed entry " << seq << " with reqId "
                << sentListEntry.reqId.ip() << ":"
                << sentListEntry.reqId.pid() << ":"
                << sentListEntry.reqId.timestamp().seconds() << " with result " << result << endl;
            cout << "...(RWAB) Printing replay log again" << endl;
            Tables::replayLog.printRelayLogContent();
            
            while (server::state == server::TAIL) {    
                //Relay to previous nodes
                grpc::ClientContext relay_context;
                google::protobuf::Empty RelayWriteAckReply;
                server::RelayWriteAckRequest request;
                request.set_seqnum(seq);
                server::ClientRequestId *clientRequestId = request.mutable_clientrequestid();
                clientRequestId->set_ip(sentListEntry.reqId.ip());
                clientRequestId->set_pid(sentListEntry.reqId.pid());
                google::protobuf::Timestamp *timestamp = clientRequestId->mutable_timestamp();
                timestamp->set_seconds(sentListEntry.reqId.timestamp().seconds());
                timestamp->set_nanos(sentListEntry.reqId.timestamp().nanos());

                grpc::Status status = server::upstream->stub->RelayWriteAck(&relay_context, request, &RelayWriteAckReply);
                cout << "...(RWAB) Forward attempt to " << server::upstream->ip << ":"
                    << server::upstream->port << " returned: " << status.error_code() << endl;
                if (status.ok()) break;
                server::build_node_stub(server::upstream);
//                sleep(1);
            }
            cout << "...(RWAB) Finished processing next entry" << endl;
        }
        catch (...) {}
    }
    cout << "ENDING RELAY WRITE ACK BACKGROUND" << endl;
}

void print_usage(){
    std::cout << "Usage: prog -master <master IP addy> (required)>\n" <<
                              "-port <my port>"
                              "-clean (initialize volume)"
                              "-v <volume name>";
}
/**
 Parse out arguments sent into program
 */
int parse_args(int argc, char** argv){
    int arg = 1;
    std::string argx;
    while (arg < argc){
        if (argc < arg) return 0;
        argx = std::string(argv[arg]);
        if (argx == "-master") {
            master_ip = std::string(argv[++arg]);
        } else if (argx == "-port") {
            my_port =  stoi(std::string(argv[++arg]));
        } else if (argx == "-ip") {
            server::my_ip =  std::string(argv[++arg]);
        } else if (argx == "-clean") {
            start_clean = true;
        } else if (argx == "-v") {
            def_volume_name = std::string(argv[++arg]);            
        } else {
            print_usage();
            return -1;
        }
        arg++;
    }
    if (master_ip == "") {
        print_usage();
        return -1;
    }
    return 0;
}


int main(int argc, char *argv[]) {
    server::state = server::INITIALIZE;
    if (parse_args(argc, argv) < 0) return -1;
    if (start_clean) Storage::init_volume(def_volume_name);
    else Storage::open_volume(def_volume_name);
    // TODO: Need to initialize sequence numbers here based on volume
    Tables::commitSeq = Storage::get_sequence_number(); // last commited write.  relay_write_ack must commit commitSeq+1
    Tables::writeSeq = Tables::commitSeq; // Sequence number of last item written.  Pending list must pull writeSeq + 1
    Tables::currentSeq = Tables::writeSeq + 1; // Next available seq number to assign to new write

    // Write relay and commit ack threads
    std::thread relay_write_thread(relay_write_background);

    // Start listening to master - TODO: Probably doesn't need to launch as thread
    std::string my_address(server::my_ip + ":" + to_string(my_port));
    server::MasterListenerImpl masterListenerImpl;
    server::NodeListenerImpl nodeListenerImpl;
    grpc::ServerBuilder listenerBuilder;
    listenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    listenerBuilder.RegisterService(&masterListenerImpl);
    listenerBuilder.RegisterService(&nodeListenerImpl);
    std::unique_ptr<grpc::Server> listener(listenerBuilder.BuildAndStart());
    std::thread listener_service_thread(server::run_service, listener.get(), "listen to master and other nodes");

    // Register node with master
    timespec start, end;
    set_time(&start);
    if (register_server() < 0) return -1;
    set_time(&end);
    double elapsed = difftimespec_ns(start, end);
    cout << "Registration took " << elapsed/1000000 << " ms." << endl;

    // Close server
    listener_service_thread.join();
    relay_write_thread.join();
    delete server::downstream;
    delete server::upstream;
    return 0;
}
