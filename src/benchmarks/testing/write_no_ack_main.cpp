#include "client_api.hpp"
#include "constants.hpp"
#include "server.h"

#include <chrono>
#include <thread>

int num_runs = 10;
string master_ip = "0.0.0.0";
string my_ip = "0.0.0.0";


void print_usage(){
    std::cout << "Usage: prog -master <master IP addy> (required)>\n" <<
              "-ip <my ip>\n"
              "-n <number of blocks written>" << endl;
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
        } else if (argx == "-ip") {
            my_ip =  std::string(argv[++arg]);
        } else if (argx == "-n") {
            num_runs =  stoi(std::string(argv[++arg]));
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
    return 0;
}

int main(int argc, char *argv[]) {
    parse_args(argc, argv);
    Client c = Client(master_ip, my_ip);
    server::ChecksumReply cs_reply;
    cs_reply = c.checksum();
    cout << "Checksum 1: " << cs_reply.chk_sum() << endl;
    if (cs_reply.valid()) cout << "...Servers consistent" << endl;
    else cout << "..Servers inconsistent" << endl;

    Client::DataBlock data, data_resp;
    google::protobuf::Timestamp tf;
    for (int i = 0; i < num_runs; i++) {

        for (int j = 0; j < Constants::BLOCK_SIZE; ++j){
            data.buff[j] =  'a' + (i%26);
        }


        google::protobuf::Timestamp tm;
        long m_offset = (long) i*Constants::BLOCK_SIZE;
        c.write(data, tm, m_offset);
        tf = tm;

        cout << "Write " << i+1 << "/" << num_runs << " (" << data.buff[0] << ")"
            << " to block " <<  m_offset << endl;
//        cout << "...The write data is size :" << sizeof(data.buff)/sizeof(data.buff[0])
//            << ": and comprised of '" << data.buff[0] << "'" << endl;

//        int start = true;
//        while (c.ackWrite(tm) < 0) {
////            std::cout << "...trying to ack write" << endl;
//            if (!start) sleep(1);
//            start = false;
//        }
//        cout << "...Write committed" << endl;
    }
    sleep(5);
    int start = true;
        while (c.ackWrite(tf) < 0) {
            if (!start) sleep(1);
            start = false;
        }
        cout << "...Write committed" << endl;
    cs_reply = c.checksum();
    cout << "...Checksum: " << cs_reply.chk_sum() << endl;
    if (cs_reply.valid()) cout << "...Servers consistent" << endl;
    else cout << "...Servers inconsistent" << endl;
}