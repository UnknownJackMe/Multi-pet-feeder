cmake_minimum_required(VERSION 3.16)
project(multi_pet_feeder_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mpf_domain
    ../firmware/components/core_types/core_types.cpp
    ../firmware/components/presence_fusion/presence_fusion.cpp
    ../firmware/components/access_control/access_control.cpp
)

target_include_directories(mpf_domain PUBLIC
    ../firmware/components/core_types/include
    ../firmware/components/presence_fusion/include
    ../firmware/components/access_control/include
)

add_executable(domain_tests domain_tests.cpp)
target_link_libraries(domain_tests PRIVATE mpf_domain)

enable_testing()
add_test(NAME domain_tests COMMAND domain_tests)
