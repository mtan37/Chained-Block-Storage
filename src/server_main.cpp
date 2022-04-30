#include <iostream>

#include <grpc++/grpc++.h>
#include "server.grpc.pb.h"
#include "master.grpc.pb.h"
#include <google/protobuf/empty.pb.h>

#include "constants.hpp"
#include "tables.hpp" 
#include "master.h"
#include "server.h"

#include <thread>

using namespace std;

// Global variables
string master_ip = "";
string my_ip = "0.0.0.0";
int my_port = Constants::SERVER_PORT;

namespace server {
    server::Node *downstream;
    server::Node *upstream;
    State state;

    string get_state() {
        switch (server::state) {
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
};



/**
 * Server will come up either in single node state or as intializing then tail
 * Not totally sure how this is going to work yet
 * @return
 */
int register_server() {
    // Set our state
    server::downstream = new server::Node;
    server::upstream = new server::Node;

    // call master to register self - build channel and stub
    string master_address = master_ip + ":" + Constants::MASTER_PORT;
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);


    // Need to set our own IP here to send into request
    master::RegisterRequest request;
    master::ServerIp * serverIP = request.mutable_server_ip();
    serverIP->set_ip(my_ip);
    serverIP->set_port(my_port);

    // hold reply
    master::RegisterReply reply;
    while (true){
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(master_address, grpc::InsecureChannelCredentials(), args);
        std::unique_ptr<master::NodeListener::Stub> master_stub = master::NodeListener::NewStub(channel);
        grpc::ClientContext context;
        grpc::Status status = master_stub->Register(&context, request, &reply);
        // Check return status on register call
        if (!status.ok()) {
            cout << "Can't contact master at " << master_address << endl;
            sleep(1);
        } else {
            cout << "Was able to contact master" << endl;
            break;
        }
    }


    // Building communication with current tail
    if (reply.has_prev_addr()) {
        //TODO: Deal with bootstraping volume here - this actually probably wont work
        // just like this.  The true tail will need to communicate  with this node
        // to bring it up to speed, and while it does it will need to pause
        // and then we will need to set this node to tail and the old tail to mid

        master::ServerIp prev_addy_ip = reply.prev_addr();
        server::upstream->ip =  prev_addy_ip.ip();
        server::upstream->port =  prev_addy_ip.port();
        //build channel stub to upstream
        string node_addr(server::upstream->ip + ":" + to_string(server::upstream->port));
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(node_addr, grpc::InsecureChannelCredentials(), args);
        server::upstream->stub = server::NodeListener::NewStub(channel);
        //TODO: Test this connection works
        server::state = server::TAIL;
    } else { // Starting in single server mode
        server::state = server::SINGLE;
    }

    cout << "My state is " << server::get_state() << endl;
    return 0;
}

void relay_write_background() {
    while(true) {
        if ( Tables::pendingQueue.getQueueSize() ) {
            Tables::PendingQueue::pendingQueueEntry pending_entry = Tables::pendingQueue.popEntry();
            if (server::state != server::TAIL) {
                string next_node_address = server::downstream->ip + ":" + to_string(server::downstream->port);
                grpc::ChannelArguments args;
                args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
                std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(next_node_address, grpc::InsecureChannelCredentials(), args);
                std::unique_ptr<server::NodeListener::Stub> next_node_stub = server::NodeListener::NewStub(channel);
                grpc::ClientContext context;
                server::RelayWriteRequest request;

                request.set_data(pending_entry.data);
                request.set_offset(pending_entry.volumeOffset);
                request.set_seqnum(pending_entry.seqNum);
                google::protobuf::Empty RelayWriteReply;

                grpc::Status status = next_node_stub->RelayWrite(&context, request, &RelayWriteReply);

                if (!status.ok()) {
                    cout << "Failed to relay write to next node" << endl;
                    //TODO: Retry? Or put back on the pending queue
                }
            } //End forwarding if non-tail

            //TODO: write locally

            //Add to sent list
            Tables::SentList::sentListEntry sent_entry;
            sent_entry.volumeOffset = pending_entry.volumeOffset;
            //TODO: Where does file offset come from?
            sent_entry.fileOffset[0] = 0;  // Defaults to -1, 0 is valid offset

            Tables::sentList.pushEntry(pending_entry.reqId, sent_entry);

            //Add to replay log
            //TODO: This needs to be moved over to write()
            int addResult = Tables::replayLog.addToLog(pending_entry.reqId);

            if (addResult < 0) {}// means entry already exist in log or has been acked

            if (server::state == server::TAIL) {
                //TODO: commit
                //TODO: send an ack backwards?
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

// Run grpc service in a loop
void run_service(grpc::Server *server, std::string serviceName) {
    std::cout << "Starting to " << serviceName << "\n";
    server->Wait();
}

int main(int argc, char *argv[]) {
    server::state = server::INITIALIZE;
    if (parse_args(argc, argv) < 0) return -1;
    if (register_server() < 0) return -1;
    
    // Write relay and commit ack threads
    std::thread relay_write_thread(relay_write_background);

    // Start listner for master
    std::string my_address(my_ip + ":" + to_string(my_port));
    server::MasterListenerImpl myMasterListen;
//
    grpc::ServerBuilder masterListenerBuilder;
    masterListenerBuilder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
    masterListenerBuilder.RegisterService(&myMasterListen);
    std::unique_ptr<grpc::Server> masterListener(masterListenerBuilder.BuildAndStart());
////     Thread server out and start listening
    std::thread masterListener_service_thread(run_service, masterListener.get(), "Listen to Master");
////    std::cout << "Starting to run master listener" << "\n";
////    masterListener->Wait();
//
    masterListener_service_thread.join();
    relay_write_thread.join();
    delete server::downstream;
    delete server::upstream;
    return 0;
}