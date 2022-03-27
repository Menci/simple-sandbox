project(sandbox)
cmake_minimum_required(VERSION 3.0)

find_library(STDCPPFS_LIBRARY stdc++fs)
find_package(fmt)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_JS_INC})
file(GLOB SOURCE_FILES "native/*.cc" "native/*.h")
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

set(JSON_BuildTests OFF CACHE INTERNAL "")

# Include N-API wrappers
execute_process(
  COMMAND node -p "require('node-addon-api').include"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE NODE_ADDON_API_DIR
)
string(REPLACE "\n" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${NODE_ADDON_API_DIR})
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB} ${STDCPPFS_LIBRARY} fmt::fmt)
