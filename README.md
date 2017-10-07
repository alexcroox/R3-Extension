
# Arma 3 After Action Replay *Extension* Component

Extension for [R3 Server Side addon](https://github.com/alexcroox/R3/)

Being built along side the [web component](https://github.com/alexcroox/R3-Web), [addon component](https://github.com/alexcroox/R3) and the [tile generation component](https://titanmods.xyz/r3/tiler/)

Built for Windows or Linux game servers.

**Note: You don't need to build the extension yourself, a Linux and Windows build are included in the releases section of the [addon component](https://github.com/alexcroox/R3/releases)**

<a href="https://discord.gg/qcE3dRP">
    <img width="100" src="http://i0.kym-cdn.com/photos/images/original/001/243/213/52a.png" alt="Discord">
</a>

### Special thanks

[ARK] Kami for building this thing in the first place and allowing me to ditch extdb!

# Manually build for Windows

## Dependecies

#### Visual Studio 2015 Express
Download from https://www.microsoft.com/en-us/download/details.aspx?id=48146 .


#### MySQL C Connector
Download and install the C connector for MySQL from https://dev.mysql.com/downloads/connector/c/ .
Tested with version 6.1 .


Choose the architecture you want to build, you can only install one. To have both 32 and 64 bit
installed, you will have to install the 64 bit using the MSI and install the 32 bit using the zip
distribution.


#### spdlog
Download version `0.10.0` from https://github.com/gabime/spdlog/archive/v0.10.0.zip and
extract the `spdlog-0.10.0/include/spdlog` directory into `extension/include`.


#### CMake
Download and install version `3.5` or higher from https://cmake.org/download/ . Make sure
CMake is on the path.




# Manually build for Linux

This has been tested on Ubuntu 16.04 LTS with GCC 5.4.0

## Dependecies

#### MySQL C Connector
Download the correct architecture you want to build for and extract the C connector for MySQL from https://dev.mysql.com/downloads/connector/c/ .
Extract the archive somewhere, we will be referencing this folder as `MYSQL_HOME`.


#### CMake
Download and install version `3.5` or higher from https://cmake.org/download/ or just run `sudo apt-get install cmake`


#### spdlog
Download version `0.10.0` from https://github.com/gabime/spdlog/archive/v0.10.0.zip and
extract the `spdlog-0.10.0/include/spdlog` directory into `extension/include`.



## Building the extension

Update and set `MYSQL_HOME` in `extension/build/CMakeLists.txt` to match your MySQL directories.

### Windows

For 32bit run `cmake . -G "Visual Studio 14 2015"`, for 64bit run `cmake . -G "Visual Studio 14 2015 Win64"`
in `extension/build` directory. Open `r3_extension.sln` in Visual Studio.

You will have to build with `Release` configuration and `Win32` platform. `Debug` configuration
is not part of the tutorial :P .

### Linux

For 32bit run `cmake . -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32`,
for 64bit run `cmake . -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64` in extension/build directory.

Now you can run make to build the extension with `make`.



## Testing and deploying
Just put the `r3_extension.dll` into A3 install directory or into one of the loaded addons folder.

You can also use the console application to test the extension without launching Arma 3.
