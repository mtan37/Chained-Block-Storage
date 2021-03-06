# reference: https://github.com/mtan37/Replicated-Block-Store/blob/main/CMakeLists.txt

cmake_minimum_required(VERSION 3.5.1)
project(CBS C CXX)

find_package(Threads REQUIRED)

find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
if(CMAKE_CROSSCOMPILING)
    find_program(_PROTOBUF_PROTOC protoc)
else()
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

find_package(gRPC REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
set(_GRPC_GRPCPP gRPC::grpc++)
if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic")

# Proto file
foreach(_target server master)
  get_filename_component(${_target}_proto "./protos/${_target}.proto" ABSOLUTE)
  get_filename_component(${_target}_proto_path "${${_target}_proto}" PATH)

  # Generated sources
  set(${_target}_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/${_target}.pb.cc")
  set(${_target}_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${_target}.pb.h")
  set(${_target}_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${_target}.grpc.pb.cc")
  set(${_target}_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${_target}.grpc.pb.h")

  add_custom_command(
        OUTPUT "${${_target}_proto_srcs}" "${${_target}_proto_hdrs}" "${${_target}_grpc_srcs}" "${${_target}_grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
          --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
          -I "${${_target}_proto_path}"
          --plugin=protoc-gen-grpc=${_GRPC_CPP_PLUGIN_EXECUTABLE}
          "${${_target}_proto}"
        DEPENDS "${${_target}_proto}")

  # Include generated *.pb.h files
  include_directories("${CMAKE_CURRENT_BINARY_DIR}")

  add_library(${_target}_grpc_proto
    ${${_target}_grpc_srcs}
    ${${_target}_grpc_hdrs}
    ${${_target}_proto_srcs}
    ${${_target}_proto_hdrs})
  target_link_libraries(${_target}_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()

# add executables to root directory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

include_directories(src/headers)
# add all the table implementations
FILE(GLOB TableImpls ./src/tablesImpl/*.cpp)

if (USE_NONATOMIC_VOLUME)
  set(_VOLUME_CPP ./src/storage-nonatomic.cpp)
else()
  set(_VOLUME_CPP ./src/storage.cpp)
endif()

# c++ targets
foreach(_target server)
  add_executable(${_target} 
    ./src/${_target}_main.cpp
    ${TableImpls}
    ./src/headers/server.h
    ./src/interfaceImpl/serverService/HeadServiceImpl.cpp
    ./src/interfaceImpl/serverService/TailServiceImpl.cpp
    ./src/interfaceImpl/serverService/NodeListenerImpl.cpp
    ./src/interfaceImpl/serverService/MasterListenerImpl.cpp
    ./src/storage.cpp
    ./src/helper.cpp
    ./src/crash.cpp
    )

    target_compile_features(${_target} PRIVATE cxx_std_17)
  target_compile_options(${_target} PRIVATE -fPIC)

  target_link_libraries(${_target}
    server_grpc_proto master_grpc_proto
  )
endforeach()

foreach(_target master)
  add_executable(${_target} 
    ./src/${_target}_main.cpp
    ./src/headers/master.h
    ./src/interfaceImpl/masterService/NodeListenerImpl.cpp
    ./src/interfaceImpl/masterService/ClientListenerImpl.cpp
    )

  target_compile_features(${_target} PRIVATE cxx_std_17)
  target_compile_options(${_target} PRIVATE -fPIC)

  target_link_libraries(${_target}
    server_grpc_proto master_grpc_proto
  )
endforeach()

# compilre test objects
# add executables to test directory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/src/benchmarks/testing)
foreach(_target replay_log_test sent_list_test pending_queue_test storage_test crash_test crash_demo)
  add_executable(${_target} 
    ./src/benchmarks/testing/${_target}_main.cpp
    ${TableImpls}
    ./src/headers/server.h
    ${_VOLUME_CPP}
    ./src/crash.cpp
    ./src/client_api.cpp
    )

  target_compile_features(${_target} PRIVATE cxx_std_17)
  target_compile_options(${_target} PRIVATE -fPIC)

  target_link_libraries(${_target}
    server_grpc_proto master_grpc_proto
  )
endforeach()

# compilre test objects
# add executables to test directory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/src/benchmarks/testing)
foreach(_target client_test basic_write_test smart_write write_no_ack)
  add_executable(${_target} 
    ./src/benchmarks/testing/${_target}_main.cpp
    ./src/headers/master.h
    ./src/client_api.cpp
    )

  target_compile_features(${_target} PRIVATE cxx_std_17)
  target_compile_options(${_target} PRIVATE -fPIC)

  target_link_libraries(${_target}
    server_grpc_proto master_grpc_proto
  )
endforeach()



# compile benchmarks
foreach(_target storage_benchmark throughput_benchmark)
  add_executable(${_target} 
    ./src/benchmarks/${_target}.cpp
    ${TableImpls}
    ./src/headers/server.h
    ./src/headers/master.h
    ${_VOLUME_CPP}
    ./src/client_api.cpp
    )

  target_compile_features(${_target} PRIVATE cxx_std_17)
  target_compile_options(${_target} PRIVATE -fPIC)

  target_link_libraries(${_target}
    server_grpc_proto master_grpc_proto
  )
endforeach()


