#include "client_api.hpp"
#include "constants.hpp"

#include <chrono>
#include <thread>

int main(int argc, char *argv[]) {
    Client c = Client("localhost", "localhost");

    Client::DataBlock data;
    std::string write_content = "yesyes";
    memcpy(data.buff, write_content.c_str(), Constants::BLOCK_SIZE);
    google::protobuf::Timestamp tm;

    c.write(data, tm, 0);
    cout << "Client test: A write data to server. Content is " << write_content << endl;
    cout << "Client test: generated timestamp" << tm.seconds() << ":" << tm.nanos() << endl;

    // check top of the pending write list
    Client::PendingWriteEntry entry;
    c.popPendingWrite(tm, entry);
    cout << "Client test: Pop data off pending list" << endl;
    cout << "Client test: generated timestamp" << tm.seconds() << ":" << tm.nanos() << endl;
    cout << "Client test: data content: " << entry.data.buff << endl;

    // second write
    write_content = "hola~";
    memcpy(data.buff, write_content.c_str(), Constants::BLOCK_SIZE);
    c.write(data, tm, Constants::BLOCK_SIZE);
    cout << "Client test: a second write data to server. Content is " << write_content << endl;
    cout << "Client test: generated timestamp" << tm.seconds() << ":" << tm.nanos() << endl;

    // ack the second write
    while (c.ackWrite(tm) < 0) {
        cout << "Try to ack write... Resut is not committed yet " << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    c.read(data, Constants::BLOCK_SIZE);
    cout << "The read data is: " << data.buff << endl;
}
