cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{DOLCESDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DOLCESDK}/share/dolce.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define DOLCESDK to point to your SDK path!")
  endif()
endif()

project(ThreadOptimizer)
include("${DOLCESDK}/share/dolce.cmake" REQUIRED)
dolce_gen_libs(SceShellSvc_stubs
  SceShellSvc.yml
  LIB SceShellSvc_stub_weak)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

include_directories(
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
  main.c
)

target_link_libraries(${PROJECT_NAME}
  taihen_stub
  SceLibKernel_stub
  SceIofilemgr_stub 
  SceAppMgr_stub
  SceSysmodule_stub 
  SceProcessmgr_stub
  SceKernelThreadMgr_stub
  SceNotificationutil_stub
  SceSysmem_stub
  SceCtrl_stub
  SceShellSvc_stub_weak
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES LINK_FLAGS "-nostdlib"
)

dolce_create_self(${PROJECT_NAME}.suprx ${PROJECT_NAME}
  CONFIG ${CMAKE_SOURCE_DIR}/ThreadOptimizer.yml
)