# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.0)

#project name
project(SAYHELLO_LIBRARY)

#add compile options - 添加编译参数
add_compile_options(-std=c++11 -Wall)

#set CMAKE_BUILD_TYPE 编译类型(Debug)
set(CMAKE_BUILD_TYPE Debug)

# set output binary path
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# 将./include 添加到头文件搜索路径
include_directories(./base)
include_directories(./net)
include_directories(./net/poller)

# 定义SRC变量，其值为当前目录下所有的源代码文件
aux_source_directory(./base SRC_BASE)
aux_source_directory(./net SRC_NET)
aux_source_directory(./net/poller SRC_NET_POLLER)

# 编译SRC变量所代表的源代码文件，生成main可执行文件
# add_executable(main main.cpp ${SRC_BASE} ${SRC_NET} ${SRC_NET_POLLER})

# add_executable(main Reactor_test01.cpp ${SRC_BASE} ${SRC_NET} ${SRC_NET_POLLER})
add_executable(main Reactor_test05.cpp ${SRC_BASE} ${SRC_NET} ${SRC_NET_POLLER})



target_link_libraries(main pthread)
