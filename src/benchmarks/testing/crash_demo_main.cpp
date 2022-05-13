#include "client_api.hpp"
#include "constants.hpp"
#include "server.h"
#include "test.h"
#include "crash.hpp"

#include <chrono>
#include <thread>

int NUM_RUNS = 2;

int main(int argc, char *argv[]) {
    Client c = Client("192.168.1.10", "192.168.1.10");
    server::ChecksumReply cs_reply;
    cs_reply = c.checksum();
    cout << "Checksum 1: " << cs_reply.chk_sum() << endl;
    if (cs_reply.valid()) cout << "...Servers consistent" << endl;
    else cout << "..Servers inconsistent" << endl;

    Client::DataBlock data, data_resp;

//        data.buff[i] =  i%256;

//    memcpy(data.buff, write_content.c_str(), Constants::BLOCK_SIZE);
//    for (int i = 0; i < NUM_RUNS; i++) {
        
        cout << "Writing 'a' to offset 0" << endl;
        for (int j = 0; j < Constants::BLOCK_SIZE; ++j){
            data.buff[j] =  'a';
        }

        google::protobuf::Timestamp tm;

        c.write(data, tm, 0);

        cout << "Client test: Writing data to server at offset " << 0 <<  endl;
        cout << "...The write data is size :" << sizeof(data.buff)/sizeof(data.buff[0]) << ":" << endl;
        //    cout << "Client test: generated timestamp " << tm.seconds() << ":" << tm.nanos() << endl;

        sleep(2);
        while (c.ackWrite(tm) < 0) {
            std::cout << "...trying to ack write" << endl;
            sleep(1);
        }

        cout << "...Write committed" << endl;
        c.read(data_resp, Constants::BLOCK_SIZE);
        cout << "...The read data is size :" << sizeof(data_resp.buff)/sizeof(data_resp.buff[0]) << ":" << endl;

        cs_reply = c.checksum();
        cout << "...Checksum: " << cs_reply.chk_sum() << endl;
        if (cs_reply.valid()) cout << "...Servers consistent" << endl;
        else cout << "...Servers inconsistent" << endl;
        
        sleep(4);
        
        cout << "Crashing the head when writing 'b' to offset 0" << endl;
        long code = Crash::construct("192.168.1.10", 1);
        
        for (int j = 0; j < Constants::BLOCK_SIZE; ++j){
            data.buff[j] =  'b';
        }

        c.write(data, tm, code);
        
        sleep(5);
        
        cout << "Checking checksum consistency with new head" << endl;
        cs_reply = c.checksum();
        cout << "...Checksum: " << cs_reply.chk_sum() << endl;
        if (cs_reply.valid()) cout << "...Servers consistent" << endl;
        else cout << "...Servers inconsistent" << endl;
        
        cout << "Writing 'c' to offset 2" << endl;
        for (int j = 0; j < Constants::BLOCK_SIZE; ++j){
            data.buff[j] =  'c';
        }

        c.write(data, tm, 2);
        
        sleep(2);
        
        cout << "Checking checksum consistency" << endl;
        cs_reply = c.checksum();
        cout << "...Checksum: " << cs_reply.chk_sum() << endl;
        if (cs_reply.valid()) cout << "...Servers consistent" << endl;
        else cout << "...Servers inconsistent" << endl;
        
        sleep (3);
        
        cout << "Writing 'd' to offset 4096" << endl;
        for (int j = 0; j < Constants::BLOCK_SIZE; ++j){
            data.buff[j] =  'd';
        }

        c.write(data, tm, 4096);
        
        sleep (2);
        
        cout << "RESTART SERVER NOW" << endl;
        
        sleep(17);
        
        cout << "Checking checksum consistency on reintegrated node" << endl;
        cs_reply = c.checksum();
        cout << "...Checksum: " << cs_reply.chk_sum() << endl;
        if (cs_reply.valid()) cout << "...Servers consistent" << endl;
        else cout << "...Servers inconsistent" << endl;
        
//    }
}
