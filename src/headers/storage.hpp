#ifndef __STORAGE_HPP
#define __STORAGE_HPP

#include <string>
#include <vector>

namespace Storage {

// Open and initialize an existing volume, or reate empty one if name doesn't exist
void open_volume(std::string volume_name);

// Create an empty volume, deleting the existing contents if they exist
void init_volume(std::string volume_name);

// Write buf to volume, but don't commit
// On success, return the file offset so it can be added to the sent list
// On error, return the 
void write(std::string buf, long volume_offset, long file_offset[2], long sequence_number);
void write(std::vector<char> buf, long volume_offset, long file_offset[2], long sequence_number);
void write(const char* buf, long volume_offset, long file_offset[2], long sequence_number);

// Read into buf from the volume. Only reads commited data
void read(std::string buf, long volume_offset);
void read(std::vector<char> buf, long volume_offset);
void read(char* buf, long volume_offset);

// Commit a write operation by modifying metadata and updating the most-resent
// marker
void commit(long sequence_number, long file_offset[2], long volume_offset);

// Get the most recent commited sequence number
long get_sequence_number();

} // namespace Storage

#endif //__STORAGE_HPP
