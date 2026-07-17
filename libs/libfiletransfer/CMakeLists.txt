add_library(libfiletransfer STATIC src/file_transfer.cpp)
target_include_directories(libfiletransfer PUBLIC include)
if(BUILD_TESTING) add_subdirectory(tests) endif()
