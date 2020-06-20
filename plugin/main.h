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

#define MAX_THREADS 128
#define MAX_NAME_LEN 37

typedef struct ThreadOptimizerSnapshot {
	char titleid[10];
	unsigned int thread_count;
	unsigned short crc16[MAX_THREADS];
	char name[MAX_THREADS][MAX_NAME_LEN];
	SceKernelThreadEntry entry[MAX_THREADS];
	int priority[MAX_THREADS];
	int affinity[MAX_THREADS];
} ThreadOptimizerSnapshot;

typedef struct SceShellSvcCustomParams {
	void* params1;  	// optional params1
	SceSize params1Size; 	// size of optional params1
	void* params2; 		// optional params2, ex. path to audio file
	SceSize params2Size;	// size of optional params2
	void* params3;		// optional params3
	SceSize params3Size;	// size of optional params3
} SceShellSvcCustomParams;

typedef struct SceShellSvcTable {
	void *pFunc_0x00;
	void *pFunc_0x04;
	void *pFunc_0x08;
	void *pFunc_0x0C;
	void *pFunc_0x10;
	void *pFunc_0x14;
	void *pFunc_0x18;
	int(*sceShellSvcAsyncMethod)(void *obj, int asyncMethodId, SceShellSvcCustomParams* params, int a4, int* a6, void* a7);

	// more ...
} SceShellSvcTable;

void* SceShellSvc_B31E7F1C(void);
int SceShellSvc_A0B067AC(void* a1, int a2);