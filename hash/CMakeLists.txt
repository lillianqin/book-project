add_library(hash STATIC emhash7.h)
target_include_directories(hash
                           PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>
                           )
set_target_properties(hash PROPERTIES LINKER_LANGUAGE CXX)