#include "client_api.hpp"
#include "constants.hpp"

int main(int argc, char *argv[]) {
    Client c = Client("localhost", "localhost");
    Client::DataBlock data;
    memcpy(data.FixedBuffer, "yesyes", Constants::BLOCK_SIZE);
    google::protobuf::Timestamp timestamp;

    c.write(data, timestamp, 0);

}
