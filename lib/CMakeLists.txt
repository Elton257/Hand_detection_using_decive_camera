# lib/ - HandBoard headers (.hpp).
# INTERFACE library: compiles nothing, only provides the header folder to others.
add_library(handboard_headers INTERFACE)
target_include_directories(handboard_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(handboard_headers INTERFACE ${OpenCV_LIBS})
