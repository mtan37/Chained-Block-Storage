#ifndef __STORAGE_HPP
#define __STORAGE_HPP

#include <string>
#include <vector>

namespace Storage {

// Open and initialize an existing volume, or reate empty one if name doesn't exist
int open_volume(std::string volume_name);

// Create an empty volume, deleting the existing contents if they exist
int init_volume(std::string volume_name);

// Write buf to volume, but don't commit
// On success, return the file offset so it can be added to the sent list
// On error, return the 
long write(std::string buf, long volume_offset);
long write(std::vector<char> buf, long volume_offset);
long write(const char* buf, long volume_offset);

// Read into buf from the volume. Only reads commited data
int read(std::string buf, long volume_offset);
int read(std::vector<char> buf, long volume_offset);
int read(char* buf, long volume_offset);

// Commit a write operation by modifying metadata and updating the most-resent
// marker
int commit(long sequence_number, long file_offset, long volume_offset);

} // namespace Storage

#endif //__STORAGE_HPP
