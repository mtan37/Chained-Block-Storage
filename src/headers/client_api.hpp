#include <iostream>
#include <map>
#include <grpc++/grpc++.h>
#include "master.h"
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

        void read (void *buf, off_t offset);

        // return right away
        void write (void *buf, google::protobuf::Timestamp &timestamp, off_t offset);
        
        /**
         * @brief See if the first pending write in 
         * the list(ealiest) is commited on server
         * This method will return right away with the result from the tail ack grpc call
         * For a timeout retry the client application will need to implement
         * something independent
         * 
         * @param timestamp of the request
         * @return int return 0 if write if committed, and remove the timestamp from pending write list
         * -1 if write retry is not committed
         */

        int ackWrite(google::protobuf::Timestamp timestamp);

        struct DataBlock {
            char FixedBuffer[4096];
        };

        std::map<google::protobuf::Timestamp,
            DataBlock, Tables::googleTimestampComparator> pending_writes;
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

        void refreshConfig();
};