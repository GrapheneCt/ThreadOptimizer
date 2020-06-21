/*
 * Copyright (c) 2020 Graphene
 */

#include <taihen.h>
#include <dolcesdk.h>
#include <psp2/appmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/notification_util.h>
#include <psp2/io/fcntl.h>
#include <psp2/ctrl.h>

#include "main.h"

static tai_hook_ref_t g_hook_ref[2];
static SceUID g_hook[2];

static char g_titleid[10];
static char g_snapshot_path[46];
static SceBool snapshot_present = SCE_FALSE;
static unsigned int thread_count = 0, frames = 0;
static SceCtrlData data;

static ThreadOptimizerSnapshot snapshot;

int sceCtrlPeekBufferPositive2(int port, SceCtrlData *pad_data, int count);

/* Notification */

void notify(char* str1, const char* str2)
{
	while (*str2)
	{
		*str1 = *str2;
		str1++;
		*str1 = '\0';
		str1++;
		str2++;
	}
}

void sendNotification(char* buffer, int size)
{
	SceShellSvcCustomParams params;
	sceClibMemset(&params, 0, sizeof(SceShellSvcCustomParams));

	SceByte bg = 0;
	int unk = -1;

	int buf[2];
	SceShellSvc_A0B067AC(&buf, 1);

	params.params1 = &bg;
	params.params1Size = 1;
	params.params2 = buffer;
	params.params2Size = size;

	void* tptr = SceShellSvc_B31E7F1C();

	((SceShellSvcTable *)(*(uint32_t *)tptr))->sceShellSvcAsyncMethod(tptr, 0x70007, &params, 2, &unk, &buf);
}

/* CRC16 */

unsigned short crc16(const unsigned char* data_p, unsigned char length) {
	unsigned char x;
	unsigned short crc = 0xFFFF;

	while (length--) {
		x = crc >> 8 ^ *data_p++;
		x ^= x >> 4;
		crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
	}
	return crc;
}

/* Threads */

SceUID sceKernelCreateThreadForUser_patched_read(const char* name,
	SceKernelThreadEntry entry,
	int initPriority,
	int stackSize,
	SceUInt attr,
	int cpuAffinityMask,
	const SceKernelThreadOptParam* option)
{
	unsigned int entry_number = 0;
	int new_priority = initPriority;
	int new_affinity = cpuAffinityMask;

	for (; entry_number < snapshot.thread_count; entry_number++) {
		if (entry == snapshot.entry[entry_number])
			break;
	}

	new_priority = snapshot.priority[entry_number];
	new_affinity = snapshot.affinity[entry_number];

	DEBUG_PRINT("----- NEW THREAD: CHANGED -----\n");
	DEBUG_PRINT("ENTRY: 0x%x\n", entry);
	DEBUG_PRINT("NAME: %s\n", name);
	DEBUG_PRINT("PRIORITY: 0x%x\n", new_priority);
	DEBUG_PRINT("AFFINITY: 0x%x\n", new_affinity);

	thread_count++;

	return TAI_CONTINUE(SceUID, g_hook_ref[0], name, entry, new_priority, stackSize, attr, new_affinity, option);
}

SceUID sceKernelCreateThreadForUser_patched_write(const char* name,
	SceKernelThreadEntry entry,
	int initPriority,
	int stackSize,
	SceUInt attr,
	int cpuAffinityMask,
	const SceKernelThreadOptParam* option)
{

	/* Calculate crc16 */

	unsigned short crc16_res = 0;
	unsigned char buffer[49];
	sceClibMemset(&buffer, 0, 49);

	sceClibMemcpy(&buffer, name, sceClibStrnlen(name, MAX_NAME_LEN));
	sceClibMemcpy(&buffer[37], &entry, 4);
	sceClibMemcpy(&buffer[41], &initPriority, 4);
	sceClibMemcpy(&buffer[45], &cpuAffinityMask, 4);
	crc16_res = crc16(buffer, 49);
	DEBUG_PRINT("CRC16: 0x%x\n", crc16_res);

	for (int i = 0; i < thread_count; i++) {
		if (crc16_res == snapshot.crc16[i]) {
			DEBUG_PRINT("THREAD ALREADY SAVED\n");
			return TAI_CONTINUE(SceUID, g_hook_ref[0], name, entry, initPriority, stackSize, attr, cpuAffinityMask, option);
		}
	}

	snapshot.crc16[thread_count] = crc16_res;
	snapshot.thread_count = thread_count;
	snapshot.entry[thread_count] = entry;
	sceClibStrncpy(snapshot.name[thread_count], name, MAX_NAME_LEN);
	snapshot.priority[thread_count] = initPriority;
	snapshot.affinity[thread_count] = cpuAffinityMask;

	DEBUG_PRINT("----- NEW THREAD: SAVED -----\n");
	DEBUG_PRINT("ENTRY: 0x%x\n", snapshot.entry[thread_count]);
	DEBUG_PRINT("NAME: %s\n", snapshot.name[thread_count]);
	DEBUG_PRINT("PRIORITY: 0x%x\n", snapshot.priority[thread_count]);
	DEBUG_PRINT("AFFINITY: 0x%x\n", snapshot.affinity[thread_count]);

	thread_count++;

	return TAI_CONTINUE(SceUID, g_hook_ref[0], name, entry, initPriority, stackSize, attr, cpuAffinityMask, option);
}

/* Save to snapshot file */

int saveToFile(void) 
{
	char snapshot_dir[35];
	sceClibMemset(&snapshot_dir, 0, 35);
	sceClibStrncpy(snapshot_dir, g_snapshot_path, 34);

	sceIoMkdir(snapshot_dir, 0777);
	SceUID fd = sceIoOpen(g_snapshot_path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
	sceIoWrite(fd, &snapshot, sizeof(ThreadOptimizerSnapshot));

	return sceIoClose(fd);
}

/* Delete snapshot file */

int deleteFile(void)
{
	char snapshot_dir[35];
	sceClibMemset(&snapshot_dir, 0, 35);
	sceClibStrncpy(snapshot_dir, g_snapshot_path, 34);

	sceIoRemove(g_snapshot_path);

	return sceIoRmdir(snapshot_dir);
}

/* Check input */

void checkInput(void)
{
	sceClibMemset(&data, 0, sizeof(SceCtrlData));
	sceCtrlPeekBufferPositive2(0, &data, 1);

	if (((data.buttons & (SCE_CTRL_R1 | SCE_CTRL_L1 | SCE_CTRL_SQUARE)) == (SCE_CTRL_R1 | SCE_CTRL_L1 | SCE_CTRL_SQUARE)) && !snapshot_present) {
		char buffer[60];
		sceClibMemset(&buffer, 0, 60);
		notify(buffer, "TO: Snapshot has been saved!");
		snapshot_present = SCE_TRUE;
		saveToFile();
		sendNotification(buffer, 60);
	}
	else if (((data.buttons & (SCE_CTRL_R1 | SCE_CTRL_L1 | SCE_CTRL_TRIANGLE)) == (SCE_CTRL_R1 | SCE_CTRL_L1 | SCE_CTRL_TRIANGLE)) && snapshot_present) {
		char buffer[67];
		sceClibMemset(&buffer, 0, 67);
		notify(buffer, "TO: Snapshot has been deleted!");
		snapshot_present = SCE_FALSE;
		deleteFile();
		sendNotification(buffer, 67);
	}
}

int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf* pParam, SceDisplaySetBufSync sync) 
{
	if (frames == 60) {
		frames = 0;
		checkInput();
	}

	frames++;

	return TAI_CONTINUE(int, g_hook_ref[1], pParam, sync);
}


void _start() __attribute__((weak, alias("module_start")));
int module_start(SceSize argc, const void *args)
{
	DEBUG_PRINT("ThreadOptimizer INIT\n");

	sceClibMemset(&g_hook[0], -1, 40);

	SceUID dfd = sceIoDopen("ux0:data/ThreadOptimizer");
	if (dfd < 0)
		sceIoMkdir("ux0:data/ThreadOptimizer", 0777);
	else
		sceIoDclose(dfd);

	sceClibMemset(&snapshot, 0, sizeof(ThreadOptimizerSnapshot));

	/* Get titleid and set up path */

	sceClibMemset(&g_titleid, 0, 10);
	sceAppMgrAppParamGetString(0, 12, g_titleid, 10);

	sceClibMemset(&g_snapshot_path, 0, 46);
	sceClibSnprintf(g_snapshot_path, 46, "ux0:data/ThreadOptimizer/%s/config.dat", g_titleid);

	/* Check for saved snapshot and load it */

	SceUID fd = 0;
	fd = sceIoOpen(g_snapshot_path, SCE_O_RDONLY, 0777);
	if (fd >= 0) {
		snapshot_present = SCE_TRUE;
		sceIoRead(fd, &snapshot, sizeof(ThreadOptimizerSnapshot));
		sceIoClose(fd);
	}
	else
		sceClibStrncpy(snapshot.titleid, g_titleid, 10);

	/* Thread hooks */

	if (snapshot_present) {
		g_hook[0] = taiHookFunctionImport(
			&g_hook_ref[0],
			TAI_MAIN_MODULE,
			TAI_ANY_LIBRARY,
			0xC5C11EE7,
			sceKernelCreateThreadForUser_patched_read);
	}
	else {
		g_hook[0] = taiHookFunctionImport(
			&g_hook_ref[0],
			TAI_MAIN_MODULE,
			TAI_ANY_LIBRARY,
			0xC5C11EE7,
			sceKernelCreateThreadForUser_patched_write);
	}

	/*g_hook[1] = taiHookFunctionImport(
		&g_hook_ref[1],
		TAI_MAIN_MODULE,
		TAI_ANY_LIBRARY,
		0x1BBDE3D9,
		sceKernelDeleteThread_patched);

	g_hook[2] = taiHookFunctionImport(
		&g_hook_ref[2],
		TAI_MAIN_MODULE,
		TAI_ANY_LIBRARY,
		0x1D17DECF,
		sceKernelExitDeleteThread_patched);*/

	/* Control hooks */

	if (!snapshot_present) {
		g_hook[1] = taiHookFunctionImport(
			&g_hook_ref[1],
			TAI_MAIN_MODULE,
			TAI_ANY_LIBRARY, //SceDisplayUser
			0x7A410B64, //sceDisplaySetFrameBuf
			sceDisplaySetFrameBuf_patched);
	}

	DEBUG_PRINT("ThreadOptimizer READY\n");

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	for (int i = 0; i < 2; i++)
		if (g_hook[i] >= 0) taiHookRelease(g_hook[i], g_hook_ref[i]);
	return SCE_KERNEL_STOP_SUCCESS;
}