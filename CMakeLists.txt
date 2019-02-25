cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(poco)

set( Poco_DIR poco/Poco )
find_package( Poco REQUIRED COMPONENTS Net Util JSON XML Foundation CONFIG )

add_executable( server server.cpp hamming_code.h)
add_executable( client client.cpp hamming_code.h)
add_executable( test_hamming_code test_hamming_code.cpp )

target_link_libraries( server PUBLIC Poco::Net Poco::Util Poco::JSON Poco::XML Poco::Foundation )
target_link_libraries( client PUBLIC Poco::Net Poco::Util Poco::JSON Poco::XML Poco::Foundation )
target_link_libraries( test_hamming_code PUBLIC Poco::Net Poco::Util Poco::JSON Poco::XML Poco::Foundation )