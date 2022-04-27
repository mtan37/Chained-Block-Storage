#include <iostream>
#include <grpc++/grpc++.h>

#include "constants.hpp"
#include "tables.hpp" 
#include "master.h"
#include "server.h"

#include <thread>

using namespace std;

// Global variables
string master_ip;

namespace server {
    std::string next_node_ip;
    std::string next_node_port;
    std::string prev_node_ip;
    std::string prev_node_port;
    State state;
};

/**
 Parse out arguments sent into program
 -alt = secondary server ip addy & port
 -listn = what port we want to listen on
 */
int parse_args(int argc, char** argv){    
    if (argc < 2) {
        cout << "Usage: server <master ip> \n"; 
        return -1;
    }

    master_ip = string(argv[1]);
    return 0;
}

int register_server() {
    // call master to register self
    string master_address = master_ip + ":" + Constants::MASTER_PORT;
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
    std::shared_ptr<grpc::Channel> channel = grpc::CreateCustomChannel(master_address, grpc::InsecureChannelCredentials(), args);
    std::unique_ptr<master::NodeListener::Stub> master_stub = master::NodeListener::NewStub(channel);
    grpc::ClientContext context;
    master::RegisterRequest request;
    master::RegisterReply reply;
    grpc::Status status = master_stub->Register(&context, request, &reply);

    if (!status.ok()) {
        cout << "Can't contact master at " << master_address << endl;
        return -1;
    }
    
    //TODO: Get next node ip and port from master
    server::next_node_ip = "";
    server::next_node_port = "";
    server::prev_node_ip = "";
    server::prev_node_port = "";
    
    return 0;
}

void relay_write_background() {
    while(true) {
        if ( Tables::pendingQueueSize() ) {
            Tables::pendingQueueEntry pending_entry = Tables::popPendingQueue();
            if (server::state != server::TAIL) {
                string next_node_address = server::next_node_ip + ":" + server::next_node_port;
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
            Tables::sentListEntry sent_entry;
            sent_entry.second.volumeOffset = pending_entry.volumeOffset;
            //TODO: Where does file offset come from?
//            sent_entry.fileOffset[0] = 0;  // Defaults to -1, 0 is valid offset
            sent_entry.first = pending_entry.seqNum;
            Tables::pushSentList(sent_entry);
            
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

int main(int argc, char *argv[]) {

    if (parse_args(argc, argv) < 0) return -1;
    if (register_server() < 0) return -1;
    
    //Write relay and commit ack threads
    std::thread relay_write_thread(relay_write_background);

    return 0;
}