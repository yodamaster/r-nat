pushd %~dp0
rd /s /q workspace
md workspace
pushd %~dp0\workspace

echo make sure you have cmake in path
@set path=D:\devtool\cmake\bin;%path%
cmake ../ -G "NMake Makefiles" -Wno-dev -DCMAKE_INSTALL_PREFIX=bin -DCMAKE_BUILD_TYPE=Release

nmake install

popd
popd