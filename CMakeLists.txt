cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(poco)

set( Poco_DIR poco/Poco )
find_package( Poco REQUIRED COMPONENTS NetSSL Util JSON XML Foundation CONFIG )

add_executable( a.out main.cpp)

target_link_libraries( a.out PUBLIC Poco::NetSSL Poco::Util Poco::JSON Poco::XML Poco::Foundation )