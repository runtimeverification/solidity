file(GLOB sources "*.cpp")
file(GLOB headers "*.h")

add_library(iele ${sources} ${headers})
target_link_libraries(iele PUBLIC devcore)
