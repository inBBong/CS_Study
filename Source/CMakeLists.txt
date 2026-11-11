cmake_minimum_required(VERSION 3.20)
project(CS_Study LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 실행 파일을 build/bin 한 곳에 모은다
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${config} CONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin)
endforeach()

find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive- /wd4324)  # 4324: alignas로 구조체 크기가 늘어남(의도한 것)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)      # fopen 등 표준 함수 사용
else()
    add_compile_options(-Wall -Wextra)
endif()

# 실습 파일 하나 = 실행 파일 하나
function(add_practice prefix source)
    get_filename_component(name ${source} NAME_WE)
    set(target ${prefix}_${name})
    add_executable(${target} ${source} ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/common)
    target_link_libraries(${target} PRIVATE Threads::Threads)
endfunction()

file(GLOB CA_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/ComputerArchitecture/*.cpp)
foreach(src ${CA_SOURCES})
    add_practice(CA ${src})
endforeach()

file(GLOB OS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/OperatingSystem/*.cpp)
foreach(src ${OS_SOURCES})
    add_practice(OS ${src})
endforeach()

# 컴파일 과정 실습: 여러 파일을 따로 컴파일한 뒤 링크한다
add_executable(CA_02_compile_process
    ComputerArchitecture/02_compile_process/main.cpp
    ComputerArchitecture/02_compile_process/calculator.cpp)
target_include_directories(CA_02_compile_process PRIVATE ${CMAKE_SOURCE_DIR}/common)
