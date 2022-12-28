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

# 所有例子
####################################

######## simple
# echo
include_directories(./examples/simple/echo)
set(SRC_SIMPLE_ECHO ./examples/simple/echo/echo.cpp)

# discard
include_directories(./examples/simple/discard)
set(SRC_SIMPLE_DISCARD ./examples/simple/discard/discard.cpp)

# daytime
include_directories(./examples/simple/daytime)
set(SRC_SIMPLE_DAYTIME ./examples/simple/daytime/daytime.cpp)

# time
include_directories(./examples/simple/time)
set(SRC_SIMPLE_TIME ./examples/simple/time/TimeServer.cpp)

# chargen
include_directories(./examples/simple/chargen)
set(SRC_SIMPLE_CHARGEN ./examples/simple/chargen/chargen.cpp)

set(SRC_SIMPLE ${SRC_SIMPLE_ECHO} ${SRC_SIMPLE_DISCARD} ${SRC_SIMPLE_TIME} ${SRC_SIMPLE_CHARGEN} ${SRC_SIMPLE_DAYTIME})
#####################################

# twisted
# include_directories(./examples/twisted/finger)
# set(SRC_TWISTED_FINGER ./examples/twisted/finger/finger01.cpp)
# set(SRC_TWISTED_FINGER ./examples/twisted/finger/finger02.cpp)
# set(SRC_TWISTED_FINGER ./examples/twisted/finger/finger03.cpp)
# set(SRC_TWISTED_FINGER ./examples/twisted/finger/finger04.cpp)
# set(SRC_TWISTED_FINGER ./examples/twisted/finger/finger05.cpp)
# set(SRC_TWISTED_FINGER ./examples/twisted/finger/finger06.cpp)
# set(SRC_TWISTED_FINGER ./examples/twisted/finger/finger07.cpp)

# set(SRC_TWISTED ${SRC_TWISTED_FINGER})
#####################################

# examples
set(SRC_EXAMPLES  ${SRC_SIMPLE})
#####################################

# 编译SRC变量所代表的源代码文件，生成main可执行文件
add_executable(main main.cpp ${SRC_BASE} ${SRC_NET} ${SRC_NET_POLLER} 
                ${SRC_EXAMPLES})


target_link_libraries(main pthread)

# test库必须依赖两个动态库，分别是boost_unit_test_framework和boost_test_exec_monitor
target_link_libraries(main boost_unit_test_framework boost_test_exec_monitor)

