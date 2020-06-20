/*
 * Copyright (c) 2020 Graphene
 */

#pragma once

#define DEBUG_ON

#ifdef DEBUG_ON
#  define DEBUG_PRINT(...) sceClibPrintf(__VA_ARGS__)
#else
#  define DEBUG_PRINT(...)
#endif

#define MAX_PRESET 1024
#define CLIB_HEAP_SIZE 1 * 1024 * 1024

#define BACK_MAGIC 4294967295U

#define LEFT_MARGIN 10
#define LEFT_MARGIN_MAIN 210
#define RIGHT_MARGIN 900
#define TOP_MARGIN 110

#define CORE_X 90
#define CORE_Y 200
#define CORE_SIZE 180
#define CORE_DELTA 20

#define WIDGET_BUTTON_TEXT_DX 30
#define WIDGET_BUTTON_TEXT_DY 50
#define WIDGET_BUTTON_HEIGHT 80

#define TOPBAR_TEXT_Y 40

#define MAX_THREADS 128
#define MAX_NAME_LEN 37

#define TOTAL_PER_PAGE 6

typedef enum PriorityType {
	PRIORITY_TYPE_SYSTEM,
	PRIORITY_TYPE_USER
} PriorityType;

typedef enum EditMode {
	MODE_AFFINITY,
	MODE_PRIORITY
} EditMode;

typedef struct ThreadOptimizerSnapshot {
	char titleid[10];
	unsigned int thread_count;
	unsigned short crc16[MAX_THREADS];
	char name[MAX_THREADS][MAX_NAME_LEN];
	SceKernelThreadEntry entry[MAX_THREADS];
	int priority[MAX_THREADS];
	int affinity[MAX_THREADS];
} ThreadOptimizerSnapshot;