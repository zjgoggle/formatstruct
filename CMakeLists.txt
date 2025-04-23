cmake_minimum_required( VERSION 3.13 )
project(formatstruct-test)
set(PROJNAME formatstruct-test)

message("Building MyCpp ${ARCHITECTURE}-${CMAKE_BUILD_TYPE}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>) # use expression to avoid msvc to append config subdir
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)


##======================================================================
##        formatstruct-test
##======================================================================
file(GLOB SRC
    src/*.h
    tests/*.cpp)

set( targetname ${PROJNAME} )
add_executable( ${targetname} ${SRC} )
target_compile_features( ${targetname} PUBLIC cxx_std_20 )
target_compile_definitions( ${targetname} PRIVATE TEST_CONFIG_IMPLEMENT_MAIN )
# target_link_libraries(  ${targetname} -static-libstdc++ -static-libgcc)
#set(CMAKE_CXX_FLAGS "--coverage")
target_include_directories( ${targetname} SYSTEM PRIVATE extern/boostpfr/include extern/magic_enum/include extern/doctest src)
