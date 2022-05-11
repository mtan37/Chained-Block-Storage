#ifndef constants_hpp
#define constants_hpp

namespace Constants {
    
    const int BLOCK_SIZE = 4096; // 4kb 

    const int HEAD_PORT_OFFSET = 1000;
    const int TAIL_PORT_OFFSET = 2000;

    const std::string MASTER_PORT = "29297";
    const int SERVER_PORT = 18001;
};
#endif