rm -r ./workspace
mkdir ./workspace
cd ./workspace

cmake ../ -G "Unix Makefiles" -Wno-dev -DCMAKE_INSTALL_PREFIX=bin -DCMAKE_BUILD_TYPE=Release

make install

cd ..
