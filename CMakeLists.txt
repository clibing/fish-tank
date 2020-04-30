# The following four lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fish_tank)

target_add_binary_data(fish_tank.elf "main/root_ca.pem" TEXT)
