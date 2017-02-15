@echo create,enter build folder
md workspace
pushd %~dp0\workspace

echo make sure you have cmake in path
@set path=D:\devtool\cmake\bin;%path%
cmake ../ -G "NMake Makefiles" -Wno-dev -DCMAKE_INSTALL_PREFIX=bin -DCMAKE_BUILD_TYPE=Release

@echo build with nmake
nmake install

popd