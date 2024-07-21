
add_executable(hello_world hello_world.cpp)

# Declare dependency on the absl::strings library
target_link_libraries(hello_world absl::strings)