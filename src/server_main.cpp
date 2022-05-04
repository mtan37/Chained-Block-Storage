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

#include <thread>

using namespace std;

// Global variables
string master_ip = "";
string my_ip = "0.0.0.0";
int my_port = Constants::SERVER_PORT;

namespace Tables {
    int currentSeq = 0;
};

namespace server {
    server::Node *downstream;
    server::Node *upstream;
    std::mutex changemode_mtx;
    State state;
    std::unique_ptr<grpc::Server> headService;
    std::unique_ptr<grpc::Server> tailService;


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
    };

    // Run grpc service in a loop
    void run_service(grpc::Server *server, std::string serviceName) {
        std::cout << "Starting to " << serviceName << "\n";
        server->Wait();
        cout << "Will no longer " << serviceName << endl;
    }

    /*
     * Launch tail service.  Service can be launched multiple times per server based on
     * failure scenarios, and must be shutdown appropriately when no longer tail.
     */
    void launch_tail(){
        std::string my_address(my_ip + ":" + to_string(my_port));
        server::TailServiceImpl tailServiceImpl;
        grpc::ServerBuilder tailBuilder;
        tailBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
        tailBuilder.RegisterService(&tailServiceImpl);
        // Thread server out and start listening
        server::tailService = tailBuilder.BuildAndStart();
        //server::tailService(tailBuilder.BuildAndStart());
//        std::thread tail_service_thread(server::run_service, tailService.get(), "listen as tail");
        std::thread (server::run_service, tailService.get(), "listen as tail").detach();
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
        std::string my_address(my_ip + ":" + to_string(my_port));
        cout << "About to launch head at " << my_address << endl;
        server::HeadServiceImpl headServiceImpl;
        grpc::ServerBuilder headBuilder;
        headBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
        headBuilder.RegisterService(&headServiceImpl);
        // Thread server out and start listening
        server::headService = headBuilder.BuildAndStart();
        std::thread (server::run_service, server::headService.get(), "listen as head").detach();
    }

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
    serverIP->set_ip(my_ip);
    serverIP->set_port(my_port);
    //TODO: We need to identify true last sequence number
    // might be able to use get_sequence_number() in storage.cpp?
    request.set_last_seq_num(0);
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
            Tables::PendingQueue::pendingQueueEntry pending_entry = Tables::pendingQueue.popEntry();
            while (server::state != server::TAIL) {
            
                grpc::ClientContext context;
                server::RelayWriteRequest request;

                request.set_data(pending_entry.data);
                request.set_offset(pending_entry.volumeOffset);
                request.set_seqnum(pending_entry.seqNum);
                google::protobuf::Empty RelayWriteReply;
                server::ClientRequestId *clientRequestId = request.mutable_clientrequestid();
                clientRequestId->set_ip(pending_entry.reqId.ip());
                clientRequestId->set_pid(pending_entry.reqId.pid());
                google::protobuf::Timestamp timestamp = *clientRequestId->mutable_timestamp();
                timestamp = pending_entry.reqId.timestamp();

                grpc::Status status = server::downstream->stub->RelayWrite(&context, request, &RelayWriteReply);

                if (!status.ok()) break;
                
            } //End forwarding if non-tail

            
            //Need clarification on file vs volume offset
            Storage::write(pending_entry.data, pending_entry.volumeOffset, pending_entry.seqNum);

            //Add to sent list
            Tables::SentList::sentListEntry sent_entry;
            sent_entry.volumeOffset = pending_entry.volumeOffset;
            sent_entry.fileOffset[0] = 0;  // Defaults to -1, 0 is valid offset

            Tables::sentList.pushEntry(pending_entry.seqNum, sent_entry);

            while (server::state == server::TAIL) {
                //Commit
                Storage::commit(pending_entry.seqNum, pending_entry.volumeOffset);

                Tables::replayLog.commitLogEntry(pending_entry.reqId);
                Tables::commitSeq = (int) pending_entry.seqNum;

                //send an ack backwards
                grpc::ClientContext relay_context;
                google::protobuf::Empty RelayWriteAckReply;
                server::RelayWriteAckRequest request;
                request.set_seqnum(pending_entry.seqNum);
                server::ClientRequestId *clientRequestId = request.mutable_clientrequestid();
                clientRequestId->set_ip(pending_entry.reqId.ip());
                clientRequestId->set_pid(pending_entry.reqId.pid());
                google::protobuf::Timestamp timestamp = *clientRequestId->mutable_timestamp();
                timestamp = pending_entry.reqId.timestamp();

                grpc::Status status = server::upstream->stub->RelayWriteAck(&relay_context, request, &RelayWriteAckReply);

                if (!status.ok()) break;

            }
        }
    }
}

void print_usage(){
    std::cout << "Usage: prog -master <master IP addy (required)>\n" <<
                              "-port <my port>";
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
            my_ip =  stoi(std::string(argv[++arg]));
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
//    if (argc != 2) {
//        print_usage();
//        return -1;
//    }
//    master_ip = std::string(argv[1]);
    return 0;
}


int main(int argc, char *argv[]) {
    server::state = server::INITIALIZE;
    if (parse_args(argc, argv) < 0) return -1;

    // Write relay and commit ack threads
    std::thread relay_write_thread(relay_write_background);

    // Start listening to master - TODO: Probably doesn't need to launch as thread
    std::string my_address(my_ip + ":" + to_string(my_port));
    server::MasterListenerImpl masterListenerImpl;
    grpc::ServerBuilder masterListenerBuilder;
    masterListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    masterListenerBuilder.RegisterService(&masterListenerImpl);
    std::unique_ptr<grpc::Server> masterListener(masterListenerBuilder.BuildAndStart());
    // Thread server out and start listening
//    std::cout << "Starting to listen to Master\n";
//    masterListener->Wait();
    std::thread masterListener_service_thread(server::run_service, masterListener.get(), "listen to Master");

    // Start listening to other nodes
    server::NodeListenerImpl nodeListenerImpl;
    grpc::ServerBuilder nodeListenerBuilder;
    nodeListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    nodeListenerBuilder.RegisterService(&nodeListenerImpl);
    std::unique_ptr<grpc::Server> nodeListener(masterListenerBuilder.BuildAndStart());
    std::thread nodeListener_service_thread(server::run_service, nodeListener.get(), "listen to other nodes");

    // Register node with master
    if (register_server() < 0) return -1;

    // Close server
    masterListener_service_thread.join();
    nodeListener_service_thread.join();
    relay_write_thread.join();
    delete server::downstream;
    delete server::upstream;
    return 0;
}