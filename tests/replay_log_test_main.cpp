#include <iostream>
#include <sys/time.h>
#include <grpc++/grpc++.h>
#include "server.h"
#include "tables.hpp"

using namespace std;

int addSimpleIds() {
    for (int i = 0; i < 10; i++) {
        server::ClientRequestId client_id;
        client_id.set_ip("1.1.1.1");
        client_id.set_pid(23636);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        google::protobuf::Timestamp *time = client_id.mutable_timestamp();
        time->set_seconds(tv.tv_sec);
        time->set_nanos(tv.tv_usec * 1000);
        int add_result_success = Tables::replayLog.addToLog(client_id);
        if (add_result_success < 0) {
            cout<<"add to log failed when it should success"<<endl;
            return -1;
        }

        int add_result_fail = Tables::replayLog.addToLog(client_id);
        if (add_result_fail >= 0) {
            cout<<"add to log success when it should fail"<<endl;
            return -1;
        }
    }
    Tables::replayLog.printRelayLogContent();
    return 0;
}

int main(int argc, char *argv[]) {
    if (addSimpleIds() < 0) cout<<"addSimpleIds test did not pass"<<endl;
    else cout<<"addSimpleIds test successful!"<<endl;
}