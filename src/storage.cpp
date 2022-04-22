#include <string>
#include <vector>

#include "storage.hpp"

namespace Storage {

int open_volume(std::string volume_name) {
}

int init_volume(std::string volume_name) {
}

long write(std::string buf, long volume_offset) {
}

long write(std::vector<char> buf, long volume_offset) {
}

long write(const char* buf, long volume_offset) {
}

int read(std::string buf, long volume_offset) {
}

int read(std::vector<char> buf, long volume_offset) {
}

int read(const char* buf, long volume_offset) {
}

int commit(long sequence_number, long file_offset, long volume_offset) {
}

} // namespace Storage
