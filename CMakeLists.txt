cmake_minimum_required(VERSION 3.14)
project(riscv_sim)

set(CMAKE_CXX_STANDARD 17)


file(GLOB SRC
        "src/*.h"
        "src/*.cpp"
        )

add_executable(riscv_sim ${SRC} src/test_decoder.h src/test_executor.h)
