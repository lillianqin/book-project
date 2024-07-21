find_package(Catch2 3 REQUIRED)

add_library(message STATIC message.h)
target_include_directories(message
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                           )
set_target_properties(message PROPERTIES LINKER_LANGUAGE CXX)

add_executable(message_test message_test.cpp)
target_link_libraries(message_test message bookproj_compiler_flags Catch2::Catch2WithMain)
target_compile_options(message_test PRIVATE "-fsanitize=address,undefined")
target_link_options(message_test PRIVATE "-fsanitize=address,undefined")

#install(FILES message.h DESTINATION include)

add_test(NAME message_usage COMMAND message_test)
