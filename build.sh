echo create, enter build folder
mkdir ./workspace
cd ./workspace

cmake ../ -G "Unix Makefiles" -Wno-dev -DCMAKE_INSTALL_PREFIX=bin -DCMAKE_BUILD_TYPE=Release

echo build with nmake
make install

cd ..
