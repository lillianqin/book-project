find_package(Catch2 3 REQUIRED)

add_library(datasource STATIC HistDataSource.h HistDataSourceFactory.h HistDataSourceFactory.cpp)
target_include_directories(datasource PUBLIC
						   $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
						   $<INSTALL_INTERFACE:include>)
set_target_properties(datasource PROPERTIES LINKER_LANGUAGE CXX)

# Add the library to the list of libraries to be installed
#install(TARGETS datasource DESTINATION ${CMAKE_INSTALL_LIBDIR})
#install(FILES HistDataSource.h HistDataSourceFactory.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/datasource)

add_executable(datasource_test datasource_test.cpp)
target_compile_options(datasource_test PRIVATE "-fsanitize=address,undefined" "-Werror;-Wall")
target_link_options(datasource_test PRIVATE "-fsanitize=address,undefined")
target_link_libraries(datasource_test PRIVATE datasource Catch2::Catch2WithMain)

include(CTest)
add_test(NAME datasource_test COMMAND datasource_test)