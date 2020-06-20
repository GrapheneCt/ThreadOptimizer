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
set(VITA_APP_NAME "Thread Optimizer")
set(VITA_TITLEID  "THRDOPTMA")
set(VITA_VERSION  "01.00")
set(DOLCE_MKSFOEX_FLAGS "${DOLCE_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3 -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format-truncation")

link_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
	)

add_executable(${PROJECT_NAME}
	src/main.c
	)

target_link_libraries(${PROJECT_NAME}
	vita2d_sys
	SceGxm_stub
	SceLibKernel_stub
	SceSysmem_stub
	SceKernelThreadMgr_stub
	SceAppMgr_stub 
	ScePvf_stub 
	SceDisplay_stub 
	SceSysmodule_stub 
	SceCtrl_stub
	SceIofilemgr_stub
	SceCommonDialog_stub
	SceVshBridge_stub
	SceJpeg_stub
	SceLibc_stub
	SceFios2_stub
    )
	
#set_target_properties(${PROJECT_NAME}
#  PROPERTIES LINK_FLAGS "-nostdlib"
#)

dolce_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

dolce_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
	FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/pic0.png sce_sys/pic0.png
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
    FILE tex/texture.gxt tex/texture.gxt
	FILE tex/bg.jpg tex/bg.jpg
    )
