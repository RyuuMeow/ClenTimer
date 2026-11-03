cmake_minimum_required(VERSION 3.21)
project(ClenTimer LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_add_executable(ClenTimer main.cpp ClenTimer.rc)
set_target_properties(ClenTimer PROPERTIES WIN32_EXECUTABLE TRUE)
target_compile_features(ClenTimer PRIVATE cxx_std_17)
target_compile_options(ClenTimer PRIVATE /utf-8)
target_link_libraries(ClenTimer PRIVATE Qt6::Widgets winmm)

add_custom_command(TARGET ClenTimer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ClenTimer>")
