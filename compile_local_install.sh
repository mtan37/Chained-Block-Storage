# this assumes that you installed cmake locally. 
# Change MY_INSTALL_DIR for a different installation location
export MY_INSTALL_DIR=$HOME/.local
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ..
make clean
make
