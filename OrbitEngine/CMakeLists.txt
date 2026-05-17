cmake_minimum_required(VERSION 3.20)
project(OrbitEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/sgp4/SGP4.cpp
    src/tle_parser/TLEParser.cpp
    src/pass_predictor/PassPredictor.cpp
    src/OrbitEngineAPI.cpp
)

add_library(OrbitEngine SHARED ${SOURCES})

target_include_directories(OrbitEngine PUBLIC include)

if(MSVC)
    target_compile_options(OrbitEngine PRIVATE /W4 /O2)
else()
    target_compile_options(OrbitEngine PRIVATE -Wall -O2)
endif()

set_target_properties(OrbitEngine PROPERTIES
    OUTPUT_NAME "OrbitEngine"
    VERSION ${PROJECT_VERSION}
)
