#ifndef __STORAGE_HPP
#define __STORAGE_HPP

#include <string>
#include <vector>

namespace Storage {

// return the name of storage system being used
std::string get_storage_type();

// Open and initialize an existing volume, or reate empty one if name doesn't exist
void open_volume(std::string volume_name);

// Create an empty volume, deleting the existing contents if they exist
void init_volume(std::string volume_name);

// Close the currently open volume and flush it to disk
void close_volume();

// Write buf to volume, but don't commit
void write(std::string buf, long volume_offset, long sequence_number);
void write(std::vector<char> buf, long volume_offset, long sequence_number);
void write(const char* buf, long volume_offset, long sequence_number);

// Read into buf from the volume. Only reads commited data
void read(std::string& buf, long volume_offset);
void read(std::vector<char>& buf, long volume_offset);
void read(char* buf, long volume_offset);

// Read the data written in given sequence number if it is not committed.
// Return true if the sequence number is in the uncommitted list, false otherwise
bool read_sequence_number(std::string& buf, long seq_num, long volume_offset);
bool read_sequence_number(std::vector<char>& buf, long seq_num, long volume_offset);
bool read_sequence_number(char* buf, long seq_num, long volume_offset);

// Commit a write operation by modifying metadata and updating the most-resent
// marker
void commit(long sequence_number, long volume_offset);

// Get the most recent commited sequence number
long get_sequence_number();

std::string checksum();

} // namespace Storage

#endif //__STORAGE_HPP
