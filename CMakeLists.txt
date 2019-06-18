cmake_minimum_required(VERSION 3.5)

set(PLUGIN_NAME Fundamental)

project(VCVRack${PLUGIN_NAME}Plugin)

set(CMAKE_CXX_STANDARD 14)

# Do not change the LIB_NAME!
set(LIB_NAME plugin)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.hpp)
source_group("Header Files" FILES ${HEADERS})

add_library(${LIB_NAME} MODULE ${SOURCES})

target_link_libraries(${LIB_NAME} ${CONAN_LIBS})

set_target_properties(${LIB_NAME} PROPERTIES PREFIX "")

file(GLOB LICENSE LICENSE*)
file(INSTALL plugin.json res ${LICENSE} DESTINATION dist/${PLUGIN_NAME})
install(DIRECTORY ${PROJECT_BINARY_DIR}/lib/ DESTINATION ${PROJECT_BINARY_DIR}/dist/${PLUGIN_NAME} OPTIONAL)
install(DIRECTORY ${PROJECT_BINARY_DIR}/dist/${PLUGIN_NAME}/ DESTINATION ${PLUGIN_NAME})
