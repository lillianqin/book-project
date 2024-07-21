find_package(Catch2 3 REQUIRED)
find_package(OpenSSL 3.0.0 REQUIRED)

add_library(digest STATIC sha256.cpp sha256.h)
target_include_directories(digest
                           PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>
                           )
target_link_libraries(digest OpenSSL::Crypto)
set_target_properties(digest PROPERTIES LINKER_LANGUAGE CXX)

add_executable(digest_test digest_test.cpp)
target_compile_options(digest_test PRIVATE "-fsanitize=address,undefined" "-Werror;-Wall")
target_link_options(digest_test PRIVATE "-fsanitize=address,undefined")
target_link_libraries(digest_test digest Catch2::Catch2WithMain OpenSSL::Crypto)

#install(TARGETS digest DESTINATION ${CMAKE_INSTALL_LIBDIR})
#install(FILES sha256.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/digest)

include(CTest)
add_test(NAME basic_test COMMAND digest_test)
