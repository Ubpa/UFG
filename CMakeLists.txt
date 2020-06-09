cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(UFG VERSION 0.2.0)
message(STATUS "[Project] ${PROJECT_NAME}")

include(cmake/InitUCMake.cmake)
Ubpa_InitUCMake()

Ubpa_InitProject()

Ubpa_AddDep(UGraphviz 0.1.0)

set(Ubpa_UFG_BuildDX12 TRUE CACHE BOOL "build Frame Graph for DirectX 12")

if(Ubpa_UFG_BuildDX12)
  Ubpa_AddDep(UDX12 0.0.3)
endif()

Ubpa_AddSubDirsRec(src)

Ubpa_Export(DIRECTORIES "include")
