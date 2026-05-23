add_executable(mnn-rpc-server ${CMAKE_CURRENT_LIST_DIR}/mnn-rpc-server.cpp)
target_include_directories(mnn-rpc-server PRIVATE ${MNN_INCLUDES})
target_link_libraries(mnn-rpc-server ${MNN_DEPS})
