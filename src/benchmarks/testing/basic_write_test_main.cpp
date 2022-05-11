#include "client_api.hpp"
#include "constants.hpp"
#include "server.h"

#include <chrono>
#include <thread>

int NUM_RUNS = 50;

int main(int argc, char *argv[]) {
    Client c = Client("localhost", "localhost");
    server::ChecksumReply cs_reply;
    cs_reply = c.checksum();
    cout << "Checksum 1: " << cs_reply.chk_sum() << endl;
    if (cs_reply.valid()) cout << "...Servers consistent" << endl;
    else cout << "..Servers inconsistent" << endl;

    Client::DataBlock data, data_resp;

//        data.buff[i] =  i%256;

//    memcpy(data.buff, write_content.c_str(), Constants::BLOCK_SIZE);
    for (int i = 0; i < NUM_RUNS; i++) {

        for (int j = 0; j < Constants::BLOCK_SIZE; ++j){
            data.buff[j] =  'a' + (i%26);
        }


        google::protobuf::Timestamp tm;

        c.write(data, tm, i*Constants::BLOCK_SIZE);

//        cout << "Client test: Writing data to server at offset " << i*Constants::BLOCK_SIZE <<  endl;
        cout << "...The write data is size :" << sizeof(data.buff)/sizeof(data.buff[0])
            << ": and comprised of '" << data.buff[0] << "'" << endl;
        //    cout << "Client test: generated timestamp " << tm.seconds() << ":" << tm.nanos() << endl;

//        sleep(2);
        int start = true;
        while (c.ackWrite(tm) < 0) {
//            std::cout << "...trying to ack write" << endl;
            if (!start) sleep(1);
            start = false;
        }

        cout << "...Write committed" << endl;
//        c.read(data_resp, Constants::BLOCK_SIZE);
//        cout << "...The read data is size :" << sizeof(data_resp.buff)/sizeof(data_resp.buff[0]) << ":" << endl;

    }
    cs_reply = c.checksum();
    cout << "...Checksum: " << cs_reply.chk_sum() << endl;
    if (cs_reply.valid()) cout << "...Servers consistent" << endl;
    else cout << "...Servers inconsistent" << endl;
}