find_package(Catch2 3 REQUIRED)

add_library(orderbook STATIC OrderBook.h FPPrice.h CIndex.h Symbol.h OrderCommon.h OrderBookPrinter.h OrderBookPrinter.cpp)
target_include_directories(orderbook
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                           )
set_target_properties(orderbook PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(orderbook absl::log)

add_executable(orderbook_test orderbook_test.cpp)
target_link_libraries(orderbook_test orderbook Catch2::Catch2)
target_compile_options(orderbook_test PRIVATE "-fsanitize=address,undefined")
target_link_options(orderbook_test PRIVATE "-fsanitize=address,undefined")

add_executable(symbol_test symbol_test.cpp)
target_link_libraries(symbol_test orderbook Catch2::Catch2WithMain)
target_compile_options(symbol_test PRIVATE "-fsanitize=address,undefined" "-Werror;-Wall")
target_link_options(symbol_test PRIVATE "-fsanitize=address,undefined")

add_executable(cindex_test cindex_test.cpp)
target_link_libraries(cindex_test orderbook Catch2::Catch2WithMain)

add_test(NAME cindex_test COMMAND cindex_test)
add_test(NAME symbol_test COMMAND symbol_test)
add_test(NAME orderbook_test COMMAND orderbook_test)
