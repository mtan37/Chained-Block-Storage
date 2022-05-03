# this assumes that you installed cmake locally. 
# Change MY_INSTALL_DIR for a different installation location
export MY_INSTALL_DIR=$HOME/.local
rm -r build
mkdir build
cd build
cmake -DUSE_NONATOMIC_VOLUME=0 -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ..
make clean
make
