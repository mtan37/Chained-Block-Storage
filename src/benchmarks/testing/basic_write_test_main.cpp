#include "client_api.hpp"
#include "constants.hpp"
#include "server.h"

#include <chrono>
#include <thread>


int main(int argc, char *argv[]) {
    Client c = Client("localhost", "localhost");
    server::ChecksumReply cs_reply;
    cs_reply = c.checksum();
    cout << "Checksum 1: " << cs_reply.chk_sum() << endl;
    if (cs_reply.valid()) cout << "...Servers consistent" << endl;
    else cout << "..Servers inconsistent" << endl;

    Client::DataBlock data, data_resp;
    for (int i = 0; i < Constants::BLOCK_SIZE; ++i) {
        data.buff[i] =  'a';
//        data.buff[i] =  i%256;
    }
//    memcpy(data.buff, write_content.c_str(), Constants::BLOCK_SIZE);
    google::protobuf::Timestamp tm;

    c.write(data, tm, 0);
    cout << "Client test: A write data to server." << endl;
//    cout << "Client test: generated timestamp " << tm.seconds() << ":" << tm.nanos() << endl;

    sleep(2);
    while (c.ackWrite(tm) < 0) {
        std::cout << "trying to ack write" << endl;
        sleep(1);
    }

    cout << "Write committed" << endl;
    c.read(data_resp, Constants::BLOCK_SIZE);
    cout << "The read data is :" << data.buff  << ":" << endl;


    cs_reply = c.checksum();
    cout << "Checksum 2: " << cs_reply.chk_sum() << endl;
    if (cs_reply.valid()) cout << "...Servers consistent" << endl;
    else cout << "...Servers inconsistent" << endl;
}