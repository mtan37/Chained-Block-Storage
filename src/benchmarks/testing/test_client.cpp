#include <grpc++/grpc++.h>
#include <iostream>

#include "client_status_defs.h"
#include "ebs.grpc.pb.h"
#include "ebs.h"

// Client can set up both channels at the beginning and simply switch which one
// it is sending to whenever primary changes.

static std::shared_ptr<grpc::Channel> channels[2];
static std::unique_ptr<server::HeadServiceImpl::Stub> head_stub;
static std::unique_ptr<server::TailServiceImpl::Stub> tail_stub;

static bool initialized = false;

static size_t primary_idx = 0;

// initialize both channels and stubs
int cr_init(char* ip1, char* port1, char* ip2, char* port2) {

    if (initialized) {
        return 0;
    }

    //TODO: probably need some error checking here
    //TODO: read ip addresses and ports from config file
    channels[0] = grpc::CreateChannel(std::string() + ip1 + ":" + port1, grpc::InsecureChannelCredentials());
    channels[1] = grpc::CreateChannel(std::string() + ip2 + ":" + port2, grpc::InsecureChannelCredentials());

    head_stub = server::HeadServiceImpl::NewStub(channels[0]);
    tail_stub = server::TailServiceImpl::NewStub(channels[1]);

    initialized = 1;

    return 0;
}

int cr_read(void *buf, off_t offset) {
    ebs::ReadReq request;
    request.set_offset(offset);

    ebs::ReadReply reply;

    while (true) {
        grpc::ClientContext context;
        grpc::Status status = stubs[primary_idx]->read(&context, request, &reply);

        if (status.ok()) {
            switch (reply.status()) {
                case cr_SUCCESS:
                    memcpy(buf, reply.data().data(), 4096);
                    return cr_SUCCESS;
                case cr_NOT_PRIMARY:
                    primary_idx = (primary_idx + 1) % 2;
                    break;
                default:
                    return cr_UNKNOWN_ERROR;
            }
        }
        else {
            if (channels[primary_idx]->GetState(true) == GRPC_CHANNEL_TRANSIENT_FAILURE) {
                primary_idx = (primary_idx + 1) % 2;
            }
        }
    }

    return cr_NO_SERVER;
}

int cr_write(void *buf, off_t offset) {
    std::string s_buf;
    s_buf.resize(4096);
    memcpy(const_cast<char*>(s_buf.data()), buf, 4096);

    ebs::WriteReq request;
    request.set_offset(offset);
    request.set_data(s_buf);

    ebs::WriteReply reply;

    while (true) {
        auto ping = channels[(primary_idx + 1) % 2]->GetState(true);
        grpc::ClientContext context;
        grpc::Status status = stubs[primary_idx]->write(&context, request, &reply);

        if (status.ok()) {
            switch (reply.status()) {
                case cr_SUCCESS:
                    return cr_SUCCESS;
                case cr_NOT_PRIMARY:
                    primary_idx = (primary_idx + 1) % 2;
                    break;
                default:
                    return cr_UNKNOWN_ERROR;
            }
        }
        if (channels[primary_idx]->GetState(true) == GRPC_CHANNEL_TRANSIENT_FAILURE) {
            primary_idx = (primary_idx + 1) % 2;
        }
    }

    return cr_NO_SERVER;
}
