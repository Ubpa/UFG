cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(UFG VERSION 0.3.0)
message(STATUS "[Project] ${PROJECT_NAME}")

include(cmake/InitUCMake.cmake)
Ubpa_InitUCMake()

Ubpa_InitProject()

Ubpa_AddDep(UGraphviz 0.1.4)

set(Ubpa_UFG_Build_DX12 TRUE CACHE BOOL "build Frame Graph for DirectX 12")

if(Ubpa_UFG_Build_DX12)
  Ubpa_AddDep(UDX12 0.0.5)
endif()

Ubpa_AddSubDirsRec(src)

Ubpa_Export(DIRECTORIES "include")
