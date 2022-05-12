#include <fstream>
#include <unistd.h>
#include <sys/time.h>

#include "helper.hpp"
#include "server.h"

void set_time(struct timespec* ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

double get_time_ns(struct timespec* ts){
    clock_gettime(CLOCK_MONOTONIC, ts);
//    return (double) ts->tv_sec * (double)1000000000 + (double)ts->tv_nsec;
    return (double)ts->tv_nsec;
}
double difftimespec_s(const struct timespec before, const struct timespec after) {
    return ((double)after.tv_sec - (double)before.tv_sec);
}

double difftimespec_ns(const struct timespec before, const struct timespec after)
{
    return ((double)after.tv_sec - (double)before.tv_sec) * (double)1000000000
         + ((double)after.tv_nsec - (double)before.tv_nsec);
}

void record_timestamp_to_file(std::string file_name) {
    server::benchmark_time_recorder_mtx.lock();
    std::ofstream myfile;
    myfile.open(file_name, fstream::out | fstream::app);

    if (!myfile) {
        myfile.open(file_name, fstream::out | fstream::trunc); // create new file
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    myfile << "" << tv.tv_sec << "." << tv.tv_usec << std::endl;;
    myfile.close();
    server::benchmark_time_recorder_mtx.unlock();
}


