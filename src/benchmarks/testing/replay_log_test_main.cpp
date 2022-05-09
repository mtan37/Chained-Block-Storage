#include <iostream>
#include <sys/time.h>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <grpc++/grpc++.h>
#include "server.h"
#include "tables.hpp"
#include "test.h"

using namespace std;

int addSimpleIds(int reset_replay_log) {
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

        if (reset_replay_log) {
            server::UpdateReplayLogRequest content;
            Tables::replayLog.getRelayLogContent(content);
            Tables::replayLog.initRelayLogContent(&content);
        }

        int add_result_fail = Tables::replayLog.addToLog(client_id);
        if (add_result_fail >= 0) {
            cout<<"add to log success when it should fail"<<endl;
            return -1;
        }
    }
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

int garbageCollectionTest() {
    // add client id entry
    server::ClientRequestId client_id;
    client_id.set_ip("77.1.1.1");
    client_id.set_pid(23636);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    google::protobuf::Timestamp *time = client_id.mutable_timestamp();
    time->set_seconds(tv.tv_sec);
    time->set_nanos(tv.tv_usec * 1000);
    Tables::replayLog.addToLog(client_id);

    // sleep for 1 second TODO
    int sleep_time = 1;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time * 1000 * 2));

    // do a garbage collection, and verify the entry is gone TOOD
    Tables::replayLog.cleanOldLogEntry(sleep_time);
    int commit_result_not_exist = Tables::replayLog.commitLogEntry(client_id);

    if (commit_result_not_exist != -1) {
        cout<<"garbageCollectionTest: Does not return the correct error code for commit log that does not exist"<<endl;
        cout << "returned " << commit_result_not_exist << " instead of " << -1 << endl;
        return -1;
    }

    return 0;
}

void garbageCollectionThread(int sleep_time, int iteration) {
    // overall iteration
    for (int i = 0; i < iteration; i++) {
        std::cout << "garbage collection" << ", it: " << i << std::endl;
        Tables::replayLog.cleanOldLogEntry(2);
        std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
    }
}

// one unit of workload
void burstingWordloadHelperAdd(int pid) {
    std::list<google::protobuf::Timestamp *> added;// list of added uncommit entry

    for (int i = 0; i < 100; i ++) {
        server::ClientRequestId client_id;
        client_id.set_ip("22.33.1.1");
        client_id.set_pid(pid);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        google::protobuf::Timestamp *time = client_id.mutable_timestamp();
        time->set_seconds(tv.tv_sec);
        time->set_nanos(tv.tv_usec * 1000);
        Tables::replayLog.addToLog(client_id);
        added.push_front(time);
    }

    // commit them
    for (auto entry_time: added) {
        server::ClientRequestId client_id;
        client_id.set_ip("22.33.1.1");
        client_id.set_pid(pid);
        google::protobuf::Timestamp *time = client_id.mutable_timestamp();
        time->set_seconds(entry_time->seconds());
        time->set_nanos(entry_time->nanos());
        Tables::replayLog.commitLogEntry(client_id);
    }

    // ack part of the entries
    for (auto entry_time: added) {
        int random = rand() % 10;
        if (random < 5) {
            server::ClientRequestId client_id;
            client_id.set_ip("22.33.1.1");
            client_id.set_pid(pid);
            google::protobuf::Timestamp *time = client_id.mutable_timestamp();
            time->set_seconds(entry_time->seconds());
            time->set_nanos(entry_time->nanos());
            Tables::replayLog.ackLogEntry(client_id);
        }
    }

}

void burstingWordloadThread(int sleep_time, int iteration, int pid) {
    // overall iteration
    for (int i = 0; i < iteration; i++) {
        std::cout << "bursting workload sleeptime " << sleep_time << ", it: " << i << std::endl;
        burstingWordloadHelperAdd(pid);
        std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
    }
}

void workLoadTest() {
    int x = 1;
    int y = 3;

    // start a thread simulating bursting wordload for every y seconds
    std::thread workload1 (burstingWordloadThread, y, 10, 133);
    // start a second thread simulating bursting wordload for every y - 1 seconds
    std::thread workload2 (burstingWordloadThread, y - 1, 10, 144);

    // start a thread for garbage collection initiating every x seconds
    // this time will be set short in comparison to y to increase the likelyhood of intervine
    std::thread garbage_collection (garbageCollectionThread, x, 40);

    workload1.join();
    workload2.join();
    garbage_collection.join();
}

int main(int argc, char *argv[]) {
    if (addSimpleIds(false) < 0) cout << "addSimpleIds test did not pass" << endl;
    else cout << "addSimpleIds test successful!" << endl;

    if (addOldEntry() < 0) cout << "addOldEntry test did not pass" << endl;
    else cout << "addOldEntry test successful!" << endl;

    if (ackUncommitedEntries() < 0) cout << "ackUncommitedEntries test did not pass" << endl;
    else cout << "ackUncommitedEntries test successful!" << endl;

    if (ackCommitedEntries() < 0) cout << "ackCommitedEntries test did not pass" << endl;
    else cout << "ackCommitedEntries test successful!" << endl;

    if (ackNonExistEntries() < 0) cout << "ackNonExistEntries test did not pass" << endl;
    else cout << "ackNonExistEntries test successful!" << endl;

    if (garbageCollectionTest() < 0) cout << "garbageCollectionTest test did not pass" << endl;
    else cout << "garbageCollectionTest test successful!" << endl;

    // Simulate regular workload happening the same time with garbage collection in between
    workLoadTest();

    // the log should be empty at this point
    std::cout<< "*******print log content*******" << endl;
    Tables::replayLog.printRelayLogContent();

    if (addSimpleIds(true) < 0) cout << "addSimpleIds(with replay log transfer) test did not pass" << endl;
    else cout << "addSimpleIds(with replay log transfer) test successful!" << endl;
    std::cout<< "*******print log content*******" << endl;
    Tables::replayLog.printRelayLogContent();

}
