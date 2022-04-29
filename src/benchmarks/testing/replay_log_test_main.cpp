#include <iostream>
#include <sys/time.h>
#include <grpc++/grpc++.h>
#include "server.h"
#include "tables.hpp"

using namespace std;

int addSimpleIds() {
    for (int i = 0; i < 100; i++) {
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
    //Tables::replayLog.printRelayLogContent();
    return 0;
}

int addOldEntry() {
    server::ClientRequestId client_id;
    client_id.set_ip("1.1.1.1");
    client_id.set_pid(23636);
    struct timeval tv;
    google::protobuf::Timestamp *time = client_id.mutable_timestamp();
    time->set_seconds(1);
    time->set_nanos(1);
    int add_result_fail = Tables::replayLog.addToLog(client_id);
    if (add_result_fail != -2) {
        cout<<"add to log not return the right error code -2"<<endl;
        return -1;
    }
    return 0;
}

int ackUncommitedEntries () {
    for (int i = 0; i < 10; i++) {
        server::ClientRequestId client_id;
        client_id.set_ip("2.1.1.1");
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

        // ack the commit 
        int ack_result = Tables::replayLog.ackLogEntry(client_id);
        if (ack_result != -2) {
            cout<<"ackLogEntry does not return the correct error code for ack uncommited log"<<endl;
            return -1;
        }
    }
    return 0;
}

int ackCommitedEntries () {
    for (int i = 0; i < 10; i++) {
        server::ClientRequestId client_id;
        client_id.set_ip("2.1.1.1");
        client_id.set_pid(23636);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        google::protobuf::Timestamp *time = client_id.mutable_timestamp();
        time->set_seconds(tv.tv_sec);
        time->set_nanos(tv.tv_usec * 1000);
        int add_result_success = Tables::replayLog.addToLog(client_id);
        if (add_result_success < 0) {
            cout<<"ackCommitedEntries: add to log failed when it should success"<<endl;
            return -1;
        }

        // mark entry as committed
        int commit_result_uncommitted = Tables::replayLog.commitLogEntry(client_id);
        // mark the entry again
        int commit_result_committed = Tables::replayLog.commitLogEntry(client_id);
        if (commit_result_uncommitted != 0) {
            cout<<"ackCommitedEntries: returns error when commit an uncommited log"<<endl;
            cout << "returned " << commit_result_uncommitted << " instead of " << 0 << endl;
            return -1;
        } else if (commit_result_committed != -2) {
            cout<<"ackCommitedEntries: returns wrong error code when commit an existing, commited log"<<endl;
            cout << "returned " << commit_result_committed << " instead of " << -2 << endl;
            return -1;
        }

        // ack the request
        int ack_result = Tables::replayLog.ackLogEntry(client_id);
        if (ack_result != 0) {
            cout<<"ackCommitedEntries: returns error when ack existing, commited log"<<endl;
            return -1;
        }
    }
    return 0;
}

int ackNonExistEntries () {
    server::ClientRequestId client_id;
    client_id.set_ip("7.1.1.1");
    client_id.set_pid(24636);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    google::protobuf::Timestamp *time = client_id.mutable_timestamp();
    time->set_seconds(tv.tv_sec);
    time->set_nanos(tv.tv_usec * 1000);
    int add_result_success = Tables::replayLog.addToLog(client_id);
    if (add_result_success < 0) {
        cout<<"ackNonExistEntries: add to log failed when it should success"<<endl;
        return -1;
    }

    server::ClientRequestId client_id2;
    client_id2.set_ip("7.1.1.1");
    client_id2.set_pid(24636);
    gettimeofday(&tv, NULL);
    time = client_id2.mutable_timestamp();
    time->set_seconds(tv.tv_sec);
    time->set_nanos(tv.tv_usec * 1000);

    server::ClientRequestId client_id3;
    client_id3.set_ip("7.1.1.1");
    client_id3.set_pid(24637);
    gettimeofday(&tv, NULL);
    time = client_id3.mutable_timestamp();
    time->set_seconds(tv.tv_sec);
    time->set_nanos(tv.tv_usec * 1000);

    // ack for an existing client entry, but non-existing timestamp
    int ack_result = Tables::replayLog.ackLogEntry(client_id2);
    if (ack_result != -1) {
        cout<<"ackNonExistEntries: Does not return the correct error code for ack uncommited log"<<endl;
        cout << "returned " << ack_result << "instead of " << -1 << endl;
        return -1;
    }

        // ack for an non-existing client entry
    ack_result = Tables::replayLog.ackLogEntry(client_id3);
    if (ack_result != -1) {
        cout<<"ackNonExistEntries: Does not return the correct error code for ack uncommited log"<<endl;
        cout << "returned " << ack_result << "instead of " << -1 << endl;
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (addSimpleIds() < 0) cout << "addSimpleIds test did not pass" << endl;
    else cout << "addSimpleIds test successful!" << endl;

    if (addOldEntry() < 0) cout << "addOldEntry test did not pass" << endl;
    else cout << "addOldEntry test successful!" << endl;

    if (ackUncommitedEntries() < 0) cout << "ackUncommitedEntries test did not pass" << endl;
    else cout << "ackUncommitedEntries test successful!" << endl;

    if (ackCommitedEntries() < 0) cout << "ackCommitedEntries test did not pass" << endl;
    else cout << "ackCommitedEntries test successful!" << endl;

    if (ackNonExistEntries() < 0) cout << "ackNonExistEntries test did not pass" << endl;
    else cout << "ackNonExistEntries test successful!" << endl;
}