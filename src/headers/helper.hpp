#ifndef __HELPER_H
#define __HELPER_H

#include <iostream>
void set_time(struct timespec* ts);
double get_time_ns(struct timespec* ts);
double difftimespec_s(const struct timespec before, const struct timespec after);
double difftimespec_ns(const struct timespec before, const struct timespec after);
void record_timestamp_to_file(std::string file_name);
#endif