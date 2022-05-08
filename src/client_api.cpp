#include "client_api.hpp"
#include "constants.hpp"
#include <unistd.h>
#include <exception>

// get new node config
void Client::refreshConfig() {
    // connect to the head server
    grpc::ClientContext context;
    google::protobuf::Empty request;
    master::GetConfigReply reply;
    grpc::Status status = this->master_stub->GetConfig(&context, request, &reply);
    if (!status.ok()) throw std::runtime_error("Client: Can't contact the master");

    master::ServerIp head = reply.head();
    this->head_channel = grpc::CreateChannel(
            head.ip() + ":" + to_string(head.port()), grpc::InsecureChannelCredentials());
    this->head_stub = server::HeadService::NewStub(this->head_channel);

    master::ServerIp tail = reply.tail();
    this->tail_channel = grpc::CreateChannel(
            tail.ip() + ":" + to_string(tail.port()), grpc::InsecureChannelCredentials());
    this->tail_stub = server::TailService::NewStub(this->tail_channel);
}

Client::Client(string master_ip, string client_ip) {
    this->master_ip = master_ip;
    this->master_port = Constants::MASTER_PORT;

    this->client_ip = client_ip;
    this->client_pid = getpid();

    // establish connection to master
    this->master_channel = grpc::CreateChannel(
            this->master_ip + ":" + this->master_port, grpc::InsecureChannelCredentials());
    this->master_stub = master::ClientListener::NewStub(this->master_channel);

    grpc::ClientContext context;
    google::protobuf::Empty request;
    master::GetConfigReply reply;
    grpc::Status status = this->master_stub->GetConfig(&context, request, &reply);
    if (!status.ok()) throw std::runtime_error("Client: Can't contact the master");

    master::ServerIp head = reply.head();
    this->head_channel = grpc::CreateChannel(
            head.ip() + ":" + to_string(head.port()), grpc::InsecureChannelCredentials());
    this->head_stub = server::HeadService::NewStub(this->head_channel);

    master::ServerIp tail = reply.tail();
    this->tail_channel = grpc::CreateChannel(
            tail.ip() + ":" + to_string(tail.port()), grpc::InsecureChannelCredentials());
    this->tail_stub = server::TailService::NewStub(this->tail_channel);
}

void Client::read (DataBlock &data, off_t offset) {
    // conntact the tail server for read
    while (true) {
        grpc::ClientContext context;
        server::ReadRequest request;
        server::ReadReply reply;
        request.set_offset(offset);
        grpc::Status status = this->tail_stub->Read(&context, request, &reply);

        if (status.ok()) {
            memcpy(data.buff, reply.data().data(), Constants::BLOCK_SIZE);
            return;
        }
        else refreshConfig();// trouble connect to head. Refresh config from master
    }
}

void Client::write (
        DataBlock data,
        google::protobuf::Timestamp &timestamp,
        off_t offset) {

    // add a lock here so this thread is not executed at multiple threads
    pending_write_mutex.lock();

    while (true) {
        // and send the content using grpc
        grpc::ClientContext context;
        server::WriteRequest request;
        server::WriteReply reply;

        // populate the request
        server::ClientRequestId * clientId = request.mutable_clientrequestid();
        clientId->set_ip(this->client_ip);
        clientId->set_pid(this->client_pid);
        
        // generate the timestamp
        struct timeval tv;
        gettimeofday(&tv, NULL);
        google::protobuf::Timestamp *timestamp_entry = clientId->mutable_timestamp();
        timestamp_entry->set_seconds(tv.tv_sec);
        timestamp_entry->set_nanos(tv.tv_usec * 1000);

        request.set_data(data.buff);
        request.set_offset(offset);

        grpc::Status status = this->head_stub->Write(&context, request, &reply);
        cout << status.error_code() << endl;
        if (!status.ok()) refreshConfig();
        else {
            // add to pending write
            PendingWriteEntry pending_entry; 
            pending_entry.data = data;
            pending_entry.offset = offset;
            pending_writes.insert(std::make_pair(*timestamp_entry, pending_entry));
            // return timestamp
            timestamp.set_seconds(tv.tv_sec);
            timestamp.set_nanos(tv.tv_usec * 1000);
            break;
        }
    }
    pending_write_mutex.unlock();
}

int Client::ackWrite(google::protobuf::Timestamp timestamp) {
    // call tail for ack
    while (true) {
        grpc::ClientContext context;
        server::WriteAckRequest request;
        server::WriteAckReply reply;
        server::ClientRequestId * clientId = request.mutable_clientrequestid();
        clientId->set_ip(this->client_ip);
        clientId->set_pid(this->client_pid);
        google::protobuf::Timestamp *timestamp_entry = clientId->mutable_timestamp();
        timestamp_entry->set_seconds(timestamp.seconds());
        timestamp_entry->set_nanos(timestamp.nanos());

        grpc::Status status = this->tail_stub->WriteAck(&context, request, &reply);

        if (status.ok()) {
            if (!reply.committed()) return -1;
            else return 0;
        }
        else refreshConfig();// trouble connect to head. Refresh config from master
    }
}

void Client::popPendingWrite(
    google::protobuf::Timestamp &timestamp,
    PendingWriteEntry &entry) {
    pending_write_mutex.lock();
    std::map<google::protobuf::Timestamp,
            PendingWriteEntry, Tables::googleTimestampComparator>::iterator it = 
            pending_writes.begin();
    timestamp.set_seconds(it->first.seconds());
    timestamp.set_nanos(it->first.nanos());
    entry = it->second;
    pending_writes.erase(it);
    pending_write_mutex.unlock();
}

void Client::retryTopPendingWrite(google::protobuf::Timestamp &timestamp) {

    while (true) {
        // and send the content using grpc
        grpc::ClientContext context;
        server::WriteRequest request;
        server::WriteReply reply;

        // populate the request
        server::ClientRequestId * clientId = request.mutable_clientrequestid();
        clientId->set_ip(this->client_ip);
        clientId->set_pid(this->client_pid);
        
        // get the timestamp
        google::protobuf::Timestamp *timestamp_entry = clientId->mutable_timestamp();
        std::map<google::protobuf::Timestamp,
            PendingWriteEntry, Tables::googleTimestampComparator>::iterator it = pending_writes.begin();
        timestamp_entry->set_seconds(it->first.seconds());
        timestamp_entry->set_nanos(it->first.nanos());

        request.set_data(it->second.data.buff);
        request.set_offset(it->second.offset);

        grpc::Status status = this->head_stub->Write(&context, request, &reply);
        if (!status.ok()) refreshConfig();
        else {
            // set the return timestamp
            timestamp.set_seconds(it->first.seconds());
            timestamp.set_nanos(it->first.nanos());
            break;
        }
    }
}