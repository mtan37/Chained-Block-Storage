#ifndef __CRASH_HPP
#define __CRASH_HPP

#include <string>

namespace Crash{

// Returns a crash code for the given ip and crash point that can be passed
// as an offset to crash a specific server at a specific point
long construct(std::string ip, unsigned short crash_point);

// Checks if the the given offset is a crash code, and whether it matches this
// server and crash point. If it is a match, the program will crash. If it is a
// crash code, but doesn't match, return the offset 0 so operation can 
// continue. If the offset is not a crash code, simply return it.
long check(long offset, unsigned short crash_point);

};

#endif // __CRASH_HPP
