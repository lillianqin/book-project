find_package(Catch2 3 REQUIRED)

add_library(itch50 STATIC itch50.h itch50.cpp
            itch50OrderBook.h
            itch50HistDataSource.h itch50HistDataSource.cpp
            itch50RawParser.h itch50RawParser.cpp)
target_include_directories(itch50
                           PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>
                           )
target_link_libraries(itch50 bookproj_compiler_flags message orderbook absl::log absl::cleanup)
set_target_properties(itch50 PROPERTIES LINKER_LANGUAGE CXX)
#set_property(TARGET itch50 PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(itch50_test itch50_test.cpp)
target_link_libraries(itch50_test itch50 message bookproj_compiler_flags Catch2::Catch2WithMain)
target_compile_options(itch50_test PRIVATE "-fsanitize=address,undefined" "-Werror;-Wall")
target_link_options(itch50_test PRIVATE "-fsanitize=address,undefined")

add_executable(itch50book_test itch50book_test.cpp)
target_link_libraries(itch50book_test itch50 message bookproj_compiler_flags Catch2::Catch2WithMain digest)

add_executable(itchraw_printer itch50_rawprinter.cpp)
target_link_libraries(itchraw_printer itch50 message bookproj_compiler_flags)
target_compile_options(itchraw_printer PRIVATE "-Werror;-Wall")

add_executable(itchbook_printer itch50OrderBookPrinter.cpp)
target_link_libraries(itchbook_printer bookproj_compiler_flags itch50 orderbook absl::flags_parse mimalloc-static)
target_compile_options(itchbook_printer PRIVATE "-Werror;-Wall")
#IPO/LTP causes significant slowdown here, so turn off
#set_property(TARGET itchbook_printer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
#set_property(TARGET itchbook_printer PROPERTY POSITION_INDEPENDENT_CODE ON)

#install(FILES itch50.h itch50OrderBook.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/itch50)
#install(TARGETS itch50 DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS itchraw_printer itchbook_printer DESTINATION bin)

include(CTest)
add_test(NAME itch50_test COMMAND itch50_test)
add_test(NAME itch50book_test COMMAND itch50book_test)
