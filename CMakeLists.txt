cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(UFG VERSION 0.3.1)
message(STATUS "[Project] ${PROJECT_NAME}")

include(cmake/InitUCMake.cmake)
Ubpa_InitUCMake()

Ubpa_InitProject()

Ubpa_AddDep(UGraphviz 0.1.4)

Ubpa_AddSubDirsRec(src)

Ubpa_Export(DIRECTORIES "include")
