if(NOT Ubpa_UFG_Build_DX12)
  return()
endif()

Ubpa_AddTarget(
  MODE STATIC
  SOURCE
    "${PROJECT_SOURCE_DIR}/include/UFG/DX12" 
  LIB
    Ubpa::UFG_core
    Ubpa::UDX12_core
)
