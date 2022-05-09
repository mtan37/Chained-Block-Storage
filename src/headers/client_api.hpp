#include <iostream>
#include <map>
#include <grpc++/grpc++.h>
#include "master.h"
#include "server.h"
#include "tables.hpp"
using namespace std;

/**
 * @brief This client wrapper is done in a way so benchmarking would be easy
 * 
 */
class Client {
    public:
        Client () = delete;
        Client (string master_ip, string client_ip);

        struct DataBlock {
            char buff[4096];
        };
        struct PendingWriteEntry {
            DataBlock data;
            off_t offset;
        };

        void read (DataBlock &data, off_t offset);

        // return right away
        void write (DataBlock data, google::protobuf::Timestamp &timestamp, off_t offset);
        
        /**
         * @brief See if the first pending write in 
         * the list(ealiest) is commited on server
         * This method will return right away with the result from the tail ack grpc call
         * For a timeout retry the client application will need to implement
         * something independent
         * 
         * @param timestamp of the request
         * @return int return 0 if write if committed
         * -1 if write retry is not committed
         */

        int ackWrite(google::protobuf::Timestamp timestamp);

        // pop a pending write(not acked) off the pending list
        // return the timestamp of the request that were poped
        void popPendingWrite(
            google::protobuf::Timestamp &timestamp,
            PendingWriteEntry &entry);

        // resend the top pending write to server
        // return the timestamp of the request that were retired
        void retryTopPendingWrite(google::protobuf::Timestamp &timestamp);

        // run checksum through chain
        server::ChecksumReply checksum ();
private:
        string master_ip;
        string master_port;
        string client_ip;
        pid_t client_pid;
        std::shared_ptr<grpc::Channel> master_channel;
        std::unique_ptr<master::ClientListener::Stub> master_stub;

        // connection to the head
        std::shared_ptr<grpc::Channel> head_channel;
        std::unique_ptr<server::HeadService::Stub> head_stub;

        // connection to the tail
        std::shared_ptr<grpc::Channel> tail_channel;
        std::unique_ptr<server::TailService::Stub> tail_stub;

        std::map<google::protobuf::Timestamp,
            PendingWriteEntry, Tables::googleTimestampComparator> pending_writes;
        std::mutex pending_write_mutex;// mutex for the pending 

        void refreshConfig();
};