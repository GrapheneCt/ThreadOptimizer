/*
 * Copyright (c) 2020 Graphene
 */

#include <stdlib.h>
#include <math.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/kernel/sysmem.h> 
#include <psp2/kernel/clib.h> 
#include <psp2/sysmodule.h>
#include <psp2/message_dialog.h>
#include <psp2/io/fcntl.h> 
#include <psp2/common_dialog.h>
#include <psp2/io/dirent.h> 
#include <psp2/ctrl.h>
#include <vita2d_sys.h>

#include "main.h"

ScePVoid g_mspace;

extern SceUID _vshKernelSearchModuleByName(const char *name, SceUInt64 *unk);
extern int sceAppMgrDestroyAppByAppId(int appId);

static char preset_dir[MAX_PRESET][10];
static vita2d_pvf *font, *icon_font;
static vita2d_texture *tex_bg, *tex_separator, *tex_button;

static SceCtrlData ctrl, ctrl_old;
static ThreadOptimizerSnapshot snapshot;
static SceBool cdlg_terminated = SCE_TRUE, cdlg_delete_request = SCE_FALSE, snapshot_loaded = SCE_FALSE, snapshot_changed = SCE_FALSE;;
static unsigned int priority_type = PRIORITY_TYPE_SYSTEM, selection_temp = 0, selection_snapshot = 0, 
selection_thread = 0, selection_page = 0, selection_page_mod = 0, selection_m = 0, selection_page_m = 0, selection_page_mod_m = 0;

#define ONPRESS(flag) ((ctrl.buttons & (flag)) && !(ctrl_old.buttons & (flag)))

int controlsThread(SceSize args, void *argp) 
{
	while (1) {
		ctrl_old = ctrl;
		sceCtrlReadBufferPositive(0, &ctrl, 1);
		sceKernelDelayThread(10000);
	}

	return 0;
}

int controls2Thread(SceSize args, void *argp)
{
	sceKernelDelayThread(500000);

	while (((ctrl.buttons & SCE_CTRL_RIGHT) == SCE_CTRL_RIGHT) || ((ctrl.buttons & SCE_CTRL_LEFT) == SCE_CTRL_LEFT)) {
		if ((ctrl.buttons & SCE_CTRL_RIGHT) == SCE_CTRL_RIGHT) {
			switch (priority_type) {
			case PRIORITY_TYPE_USER:
				if (snapshot.priority[selection_temp] != 0x100000E0)
					snapshot.priority[selection_temp]--;
				break;
			case PRIORITY_TYPE_SYSTEM:
				if (snapshot.priority[selection_temp] != 64)
					snapshot.priority[selection_temp]--;
				break;
			}
			sceKernelDelayThread(30000);
		}
		else if ((ctrl.buttons & SCE_CTRL_LEFT) == SCE_CTRL_LEFT) {
			switch (priority_type) {
			case PRIORITY_TYPE_USER:
				if (snapshot.priority[selection_temp] != 0x10000120)
					snapshot.priority[selection_temp]++;
				break;
			case PRIORITY_TYPE_SYSTEM:
				if (snapshot.priority[selection_temp] != 191)
					snapshot.priority[selection_temp]++;
				break;
			}
			sceKernelDelayThread(30000);
		}
	}

	sceKernelExitThread(0);
	return 0;
}

int showMessage(const char *str) 
{
	SceMsgDialogParam				msgParam;
	SceMsgDialogUserMessageParam	userMsgParam;

	sceMsgDialogParamInit(&msgParam);
	msgParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;

	sceClibMemset(&userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
	msgParam.userMsgParam = &userMsgParam;
	msgParam.userMsgParam->msg = (SceChar8 *)str;
	msgParam.userMsgParam->buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;

	msgParam.commonParam.infobarParam = NULL;
	msgParam.commonParam.bgColor = NULL;
	msgParam.commonParam.dimmerColor = NULL;

	return sceMsgDialogInit(&msgParam);
}

int showMessageYesNo(const char *str)
{
	SceMsgDialogParam				msgParam;
	SceMsgDialogUserMessageParam	userMsgParam;

	sceMsgDialogParamInit(&msgParam);
	msgParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;

	sceClibMemset(&userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
	msgParam.userMsgParam = &userMsgParam;
	msgParam.userMsgParam->msg = (SceChar8 *)str;
	msgParam.userMsgParam->buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_YESNO;

	msgParam.commonParam.infobarParam = NULL;
	msgParam.commonParam.bgColor = NULL;
	msgParam.commonParam.dimmerColor = NULL;

	return sceMsgDialogInit(&msgParam);
}

void cdlgDraw(void) {
	vita2d_start_drawing();
	vita2d_clear_screen();
	vita2d_draw_texture(tex_bg, 0, 0);
	vita2d_end_drawing();
	vita2d_wait_rendering_done();
	vita2d_common_dialog_update();
	vita2d_end_shfb();
}

float priorityToWidth(int priority_mode, int priority)
{
	if (priority_mode == PRIORITY_TYPE_SYSTEM) {
		return (191 - priority) * 6.299f;
	}
	else
		return (0x10000120 - priority) * 12.5f;
}

void drawCores(int affinity)
{
	vita2d_draw_rectangle(CORE_X, CORE_Y, CORE_SIZE, CORE_SIZE, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(CORE_X + 10, CORE_Y + 10, CORE_SIZE - 20, CORE_SIZE - 20, RGBA8(105, 121, 125, 255));
	if (affinity & SCE_KERNEL_CPU_MASK_USER_0)
		vita2d_draw_rectangle(CORE_X + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(255, 255, 255, 255));
	else
		vita2d_draw_rectangle(CORE_X + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(105, 121, 125, 255));

	vita2d_draw_rectangle(CORE_X + CORE_SIZE + CORE_DELTA, CORE_Y, CORE_SIZE, CORE_SIZE, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(CORE_X + CORE_SIZE + CORE_DELTA + 10, CORE_Y + 10, CORE_SIZE - 20, CORE_SIZE - 20, RGBA8(105, 121, 125, 255));
	if (affinity & SCE_KERNEL_CPU_MASK_USER_1)
		vita2d_draw_rectangle(CORE_X + CORE_SIZE + CORE_DELTA + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(255, 255, 255, 255));
	else
		vita2d_draw_rectangle(CORE_X + CORE_SIZE + CORE_DELTA + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(105, 121, 125, 255));

	vita2d_draw_rectangle(CORE_X + CORE_SIZE * 2 + CORE_DELTA * 2, CORE_Y, CORE_SIZE, CORE_SIZE, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(CORE_X + CORE_SIZE * 2 + CORE_DELTA * 2 + 10, CORE_Y + 10, CORE_SIZE - 20, CORE_SIZE - 20, RGBA8(105, 121, 125, 255));
	if (affinity & SCE_KERNEL_CPU_MASK_USER_2)
		vita2d_draw_rectangle(CORE_X + CORE_SIZE * 2 + CORE_DELTA * 2 + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(255, 255, 255, 255));
	else
		vita2d_draw_rectangle(CORE_X + CORE_SIZE * 2 + CORE_DELTA * 2 + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(105, 121, 125, 255));

	vita2d_draw_rectangle(CORE_X + CORE_SIZE * 3 + CORE_DELTA * 3, CORE_Y, CORE_SIZE, CORE_SIZE, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(CORE_X + CORE_SIZE * 3 + CORE_DELTA * 3 + 10, CORE_Y + 10, CORE_SIZE - 20, CORE_SIZE - 20, RGBA8(105, 121, 125, 255));
	if (affinity & SCE_KERNEL_CPU_MASK_USER_3)
		vita2d_draw_rectangle(CORE_X + CORE_SIZE * 3 + CORE_DELTA * 3 + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(255, 255, 255, 255));
	else
		vita2d_draw_rectangle(CORE_X + CORE_SIZE * 3 + CORE_DELTA * 3 + 20, CORE_Y + 20, CORE_SIZE - 40, CORE_SIZE - 40, RGBA8(105, 121, 125, 255));
}

void drawSelector(unsigned int selection)
{
	int x = CORE_X + (CORE_SIZE * selection + CORE_DELTA * selection);
	vita2d_draw_rectangle(x, CORE_Y - 40, CORE_SIZE, 5, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(x, CORE_Y - 40, 5, 10, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(x + CORE_SIZE - 5, CORE_Y - 40, 5, 10, RGBA8(255, 255, 255, 255));

	vita2d_draw_rectangle(x, CORE_Y + CORE_SIZE + 40, CORE_SIZE, 5, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(x, CORE_Y + CORE_SIZE + 35, 5, 10, RGBA8(255, 255, 255, 255));
	vita2d_draw_rectangle(x + CORE_SIZE - 5, CORE_Y + CORE_SIZE + 35, 5, 10, RGBA8(255, 255, 255, 255));
}

unsigned int drawThreadEdit(unsigned int selection_prev)
{
	selection_temp = selection_prev;
	SceUID ctrl2_thrd = sceKernelCreateThread("Ctrl2Thread", controls2Thread, 0x10000100, 0x1000, 0, 0, NULL);

	char thread_text[MAX_NAME_LEN + 8];
	sceClibSnprintf(thread_text, MAX_NAME_LEN + 8, "0x%04X %s", snapshot.crc16[selection_prev], snapshot.name[selection_prev]);

	int priority_backup, affinity_backup;
	priority_backup = snapshot.priority[selection_prev];
	affinity_backup = snapshot.affinity[selection_prev];

	unsigned int selection = 0, edit_mode = MODE_AFFINITY;

	while (1) {
		vita2d_start_drawing();
		vita2d_clear_screen();

		vita2d_draw_texture(tex_bg, 0, 0);
		vita2d_pvf_draw_text(font, 480 - (vita2d_pvf_text_width(font, 1, thread_text) / 2), TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, thread_text);
		vita2d_draw_texture(tex_separator, 0, TOPBAR_TEXT_Y + 20);

		vita2d_pvf_draw_text(icon_font, LEFT_MARGIN_MAIN, TOPBAR_TEXT_Y + 70, RGBA8(255, 255, 255, 255), 1, "'");
		vita2d_pvf_draw_text(font, LEFT_MARGIN_MAIN + 40, TOPBAR_TEXT_Y + 70, RGBA8(255, 255, 255, 255), 1, "Edit Affinity");
		vita2d_pvf_draw_text(icon_font, LEFT_MARGIN_MAIN + 180, TOPBAR_TEXT_Y + 70, RGBA8(255, 255, 255, 255), 1, "$");
		vita2d_pvf_draw_text(font, LEFT_MARGIN_MAIN + 220, TOPBAR_TEXT_Y + 70, RGBA8(255, 255, 255, 255), 1, "Edit Priority");
		vita2d_pvf_draw_text(icon_font, LEFT_MARGIN_MAIN + 360, TOPBAR_TEXT_Y + 70, RGBA8(255, 255, 255, 255), 1, "5");
		vita2d_pvf_draw_text(font, LEFT_MARGIN_MAIN + 420, TOPBAR_TEXT_Y + 70, RGBA8(255, 255, 255, 255), 1, "Restore All");

		switch (edit_mode) {
		case MODE_AFFINITY:

			drawCores(snapshot.affinity[selection_prev]);
			drawSelector(selection);

			vita2d_pvf_draw_text(font, LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 460, RGBA8(255, 255, 255, 255), 1, "*If no cores set, CPU affinity mask will be automatically assigned by the system");

			vita2d_end_drawing();
			vita2d_wait_rendering_done();
			vita2d_end_shfb();

			if (ONPRESS(SCE_CTRL_RIGHT)) {
				if (selection != 3)
					selection++;
			}
			else if (ONPRESS(SCE_CTRL_LEFT)) {
				if (selection != 0)
					selection--;
			}
			else if (ONPRESS(SCE_CTRL_CROSS)) {
				switch (selection) {
				case 0:
					snapshot.affinity[selection_prev] ^= SCE_KERNEL_CPU_MASK_USER_0;
					break;
				case 1:
					snapshot.affinity[selection_prev] ^= SCE_KERNEL_CPU_MASK_USER_1;
					break;
				case 2:
					snapshot.affinity[selection_prev] ^= SCE_KERNEL_CPU_MASK_USER_2;
					break;
				case 3:
					snapshot.affinity[selection_prev] ^= SCE_KERNEL_CPU_MASK_USER_3;
					break;
				}
			}

			break;
		case MODE_PRIORITY:

			if (snapshot.priority[selection_prev] > SCE_KERNEL_COMMON_QUEUE_LOWEST_PRIORITY)
				priority_type = PRIORITY_TYPE_USER;
			else
				priority_type = PRIORITY_TYPE_SYSTEM;

			vita2d_draw_rectangle(80, TOPBAR_TEXT_Y + 220, 800, 10, RGBA8(54, 59, 61, 255));
			vita2d_draw_rectangle(80, TOPBAR_TEXT_Y + 220, priorityToWidth(priority_type, snapshot.priority[selection_prev]), 10, RGBA8(255, 255, 255, 255));

			if (priority_type == PRIORITY_TYPE_SYSTEM) {
				vita2d_pvf_draw_text(icon_font, LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 120, RGBA8(255, 255, 255, 255), 1, "&");
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 50, TOPBAR_TEXT_Y + 120, RGBA8(255, 255, 255, 255), 1, "Priority mode: [System]");
				vita2d_pvf_draw_textf(font, LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 170, RGBA8(255, 255, 255, 255), 1, "Current priority: %d", snapshot.priority[selection_prev]);
				vita2d_draw_rectangle(80, TOPBAR_TEXT_Y + 230, 3, 10, RGBA8(0, 0, 255, 255));
				vita2d_draw_rectangle(877, TOPBAR_TEXT_Y + 230, 3, 10, RGBA8(255, 0, 0, 255));
				vita2d_draw_rectangle(480, TOPBAR_TEXT_Y + 230, 3, 10, RGBA8(0, 255, 0, 255));
				vita2d_draw_rectangle(474, TOPBAR_TEXT_Y + 230, 3, 10, RGBA8(255, 255, 0, 255));
				vita2d_draw_rectangle(LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 288, 10, 10, RGBA8(0, 0, 255, 255));
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 30, TOPBAR_TEXT_Y + 300, RGBA8(255, 255, 255, 255), 1, "SCE_KERNEL_COMMON_QUEUE_LOWEST_PRIORITY (191)");
				vita2d_draw_rectangle(LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 328, 10, 10, RGBA8(255, 255, 0, 255));
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 30, TOPBAR_TEXT_Y + 340, RGBA8(255, 255, 255, 255), 1, "SCE_KERNEL_COMMON_QUEUE_HIGHEST_PRIORITY (128)");
				vita2d_draw_rectangle(LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 368, 10, 10, RGBA8(0, 255, 0, 255));
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 30, TOPBAR_TEXT_Y + 380, RGBA8(255, 255, 255, 255), 1, "SCE_KERNEL_INDIVIDUAL_QUEUE_LOWEST_PRIORITY (127)");
				vita2d_draw_rectangle(LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 408, 10, 10, RGBA8(255, 0, 0, 255));
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 30, TOPBAR_TEXT_Y + 420, RGBA8(255, 255, 255, 255), 1, "SCE_KERNEL_INDIVIDUAL_QUEUE_HIGHEST_PRIORITY (64)");
			}
			else {
				vita2d_pvf_draw_text(icon_font, LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 120, RGBA8(255, 255, 255, 255), 1, "&");
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 50, TOPBAR_TEXT_Y + 120, RGBA8(255, 255, 255, 255), 1, "Priority mode: [User]");
				vita2d_pvf_draw_textf(font, LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 170, RGBA8(255, 255, 255, 255), 1, "Current priority: %d", snapshot.priority[selection_prev] - 0x100000A0);
				vita2d_draw_rectangle(80, TOPBAR_TEXT_Y + 230, 3, 10, RGBA8(0, 0, 255, 255));
				vita2d_draw_rectangle(877, TOPBAR_TEXT_Y + 230, 3, 10, RGBA8(255, 0, 0, 255));
				vita2d_draw_rectangle(LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 288, 10, 10, RGBA8(0, 0, 255, 255));
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 30, TOPBAR_TEXT_Y + 300, RGBA8(255, 255, 255, 255), 1, "SCE_KERNEL_INDIVIDUAL_QUEUE_LOWEST_PRIORITY (127)");
				vita2d_draw_rectangle(LEFT_MARGIN + 10, TOPBAR_TEXT_Y + 328, 10, 10, RGBA8(255, 0, 0, 255));
				vita2d_pvf_draw_text(font, LEFT_MARGIN + 30, TOPBAR_TEXT_Y + 340, RGBA8(255, 255, 255, 255), 1, "SCE_KERNEL_INDIVIDUAL_QUEUE_HIGHEST_PRIORITY (64)");
			}

			vita2d_end_drawing();
			vita2d_wait_rendering_done();
			vita2d_end_shfb();

			if (ONPRESS(SCE_CTRL_RIGHT)) {
				sceKernelStartThread(ctrl2_thrd, 0, NULL);
				switch (priority_type) {
				case PRIORITY_TYPE_USER:
					if (snapshot.priority[selection_prev] != 0x100000E0)
						snapshot.priority[selection_prev]--;
					break;
				case PRIORITY_TYPE_SYSTEM:
					if (snapshot.priority[selection_prev] != 64)
						snapshot.priority[selection_prev]--;
					break;
				}
			}
			else if (ONPRESS(SCE_CTRL_LEFT)) {
				sceKernelStartThread(ctrl2_thrd, 0, NULL);
				switch (priority_type) {
				case PRIORITY_TYPE_USER:
					if (snapshot.priority[selection_prev] != 0x10000120)
						snapshot.priority[selection_prev]++;
					break;
				case PRIORITY_TYPE_SYSTEM:
					if (snapshot.priority[selection_prev] != 191)
						snapshot.priority[selection_prev]++;
					break;
				}
			}
			else if (ONPRESS(SCE_CTRL_CROSS)) {
				if (snapshot.priority[selection_prev] > SCE_KERNEL_COMMON_QUEUE_LOWEST_PRIORITY)
					snapshot.priority[selection_prev] = 96;
				else
					snapshot.priority[selection_prev] = SCE_KERNEL_DEFAULT_PRIORITY;
			}

			break;
		}

		if (ONPRESS(SCE_CTRL_CIRCLE)) {
			break;
		}
		else if (ONPRESS(SCE_CTRL_SQUARE)) {
			edit_mode = MODE_AFFINITY;
		}
		else if (ONPRESS(SCE_CTRL_TRIANGLE)) {
			edit_mode = MODE_PRIORITY;
		}
		else if (ONPRESS(SCE_CTRL_START)) {
			snapshot.priority[selection_prev] = priority_backup;
			snapshot.affinity[selection_prev] = affinity_backup;
		}
	}

	if ((snapshot.priority[selection_prev] != priority_backup) || (snapshot.affinity[selection_prev] != affinity_backup))
		snapshot_changed = SCE_TRUE;

	return selection_snapshot;
}

unsigned int drawThreadSelect(unsigned int selection_prev)
{
	selection_snapshot = selection_prev;
	unsigned int total_pages = 0;
	char snapshot_path[46];

	if (!snapshot_loaded) {
		sceClibMemset(&snapshot, 0, sizeof(ThreadOptimizerSnapshot));

		sceClibMemset(&snapshot_path, 0, 46);
		sceClibSnprintf(snapshot_path, 46, "ux0:data/ThreadOptimizer/%s/config.dat", preset_dir[selection_prev]);

		SceUID fd = sceIoOpen(snapshot_path, SCE_O_RDONLY, 0777);
		sceIoRead(fd, &snapshot, sizeof(ThreadOptimizerSnapshot));
		sceIoClose(fd);

		snapshot_loaded = SCE_TRUE;
	}

	float total_pages_temp = (float)snapshot.thread_count / (float)TOTAL_PER_PAGE;
	total_pages = ceilf(total_pages_temp);
	total_pages--;

	vita2d_sys_widget* widget[MAX_THREADS];

	int coordinate_mltp = 0;
	char thread_text[MAX_THREADS][MAX_NAME_LEN + 8];

	for (int i = 0; i < snapshot.thread_count; i++) {
		sceClibSnprintf(thread_text[i], MAX_NAME_LEN + 8, "0x%04X %s", snapshot.crc16[i], snapshot.name[i]);
		widget[i] = vita2d_sys_create_widget_button(tex_button, font, 0, (TOPBAR_TEXT_Y + 22) + WIDGET_BUTTON_HEIGHT * coordinate_mltp, WIDGET_BUTTON_TEXT_DX, WIDGET_BUTTON_TEXT_DY, thread_text[i]);
		coordinate_mltp++;
		if (coordinate_mltp == TOTAL_PER_PAGE)
			coordinate_mltp = 0;
	}

	unsigned int selection = selection_thread, page = selection_page, page_modifier = selection_page_mod;

	if (snapshot.thread_count == 0) {
		showMessageYesNo("Error: There are no threads in this snapshot.\n\nDo you want to delete it?");
		while (sceMsgDialogGetStatus() != 2)
			cdlgDraw();
		SceMsgDialogResult result;
		sceMsgDialogGetResult(&result);
		sceMsgDialogTerm();
		if (result.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES) {
			char snapshot_dir_path[35];
			sceClibMemset(&snapshot_dir_path, 0, 35);
			sceClibSnprintf(snapshot_dir_path, 35, "ux0:data/ThreadOptimizer/%s/config.dat", preset_dir[selection_prev]);
			sceIoRemove(snapshot_path);
			sceIoRmdir(snapshot_dir_path);
		}
		selection = BACK_MAGIC;
		goto threadSelect_ret;
	}

	while (1) {
		vita2d_start_drawing();
		vita2d_clear_screen();

		vita2d_draw_texture(tex_bg, 0, 0);
		vita2d_pvf_draw_text(font, 480 - (vita2d_pvf_text_width(font, 1, "Thread Selection") / 2), TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "Thread Selection");
		if (page != 0)
			vita2d_pvf_draw_text(icon_font, LEFT_MARGIN, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "0");
		if (page != total_pages)
			vita2d_pvf_draw_text(icon_font, RIGHT_MARGIN, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "1");
		vita2d_pvf_draw_text(icon_font, LEFT_MARGIN + 60, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "'");
		vita2d_pvf_draw_text(font, LEFT_MARGIN + 60, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "    Save");
		vita2d_pvf_draw_text(icon_font, RIGHT_MARGIN - 40, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "$");
		vita2d_pvf_draw_text(font, RIGHT_MARGIN - 150, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "    Delete");
		vita2d_draw_texture(tex_separator, 0, TOPBAR_TEXT_Y + 20);

		for (int i = 0; i < TOTAL_PER_PAGE && ((i + page_modifier) < snapshot.thread_count); i++) {
			if (i == selection)
				vita2d_sys_widget_set_highlight(widget[i + page_modifier], SCE_TRUE);
			else
				vita2d_sys_widget_set_highlight(widget[i + page_modifier], SCE_FALSE);
			vita2d_sys_draw_widget(widget[i + page_modifier]);
		}

		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		if (sceMsgDialogGetStatus() != 2 && !cdlg_terminated)
			vita2d_common_dialog_update();
		else if (!cdlg_terminated) {
			sceMsgDialogTerm();
			cdlg_terminated = SCE_TRUE;
		}
		vita2d_end_shfb();

		if (cdlg_delete_request) {
			SceMsgDialogResult result;
			sceMsgDialogGetResult(&result);
			if (result.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES) {
				sceMsgDialogTerm();
				cdlg_terminated = SCE_TRUE;
				cdlg_delete_request = SCE_FALSE;
				char snapshot_dir_path[35];
				sceClibMemset(&snapshot_dir_path, 0, 35);
				sceClibSnprintf(snapshot_dir_path, 35, "ux0:data/ThreadOptimizer/%s/config.dat", preset_dir[selection_prev]);
				sceIoRemove(snapshot_path);
				sceIoRmdir(snapshot_dir_path);
				selection = BACK_MAGIC;
				break;
			}
			else if (result.buttonId == SCE_MSG_DIALOG_BUTTON_ID_NO) {
				sceMsgDialogTerm();
				cdlg_terminated = SCE_TRUE;
				cdlg_delete_request = SCE_FALSE;
			}
		}

		if (ONPRESS(SCE_CTRL_DOWN)) {
			if ((selection != (TOTAL_PER_PAGE - 1)) && ((selection + page_modifier) != (snapshot.thread_count - 1))) {
				vita2d_sys_widget_set_highlight_max();
				selection++;
			}
		}
		else if (ONPRESS(SCE_CTRL_UP)) {
			if (selection != 0) {
				vita2d_sys_widget_set_highlight_max();
				selection--;
			}
		}
		else if (ONPRESS(SCE_CTRL_R2)) {
			if (page != total_pages) {
				vita2d_sys_widget_set_highlight_max();
				page_modifier += TOTAL_PER_PAGE;
				selection = 0;
				page++;
			}
		}
		else if (ONPRESS(SCE_CTRL_L2)) {
			if (page != 0) {
				vita2d_sys_widget_set_highlight_max();
				page_modifier -= TOTAL_PER_PAGE;
				selection = 0;
				page--;
			}
		}
		else if (ONPRESS(SCE_CTRL_CROSS)) {
			for (int i = 0; i < snapshot.thread_count; i++)
				vita2d_sys_delete_widget(widget[i]);
			break;
		}
		else if (ONPRESS(SCE_CTRL_CIRCLE)) {
			for (int i = 0; i < snapshot.thread_count; i++)
				vita2d_sys_delete_widget(widget[i]);
			if (snapshot_changed) {
				showMessageYesNo("Do you want to save changes?");
				while (sceMsgDialogGetStatus() != 2)
					cdlgDraw();
				SceMsgDialogResult result;
				sceMsgDialogGetResult(&result);
				sceMsgDialogTerm();
				if (result.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES) {
					SceUID fd = sceIoOpen(snapshot_path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
					sceIoWrite(fd, &snapshot, sizeof(ThreadOptimizerSnapshot));
					sceIoClose(fd);
				}
				snapshot_changed = SCE_FALSE;
			}
			selection = BACK_MAGIC;
			break;
		}
		else if (ONPRESS(SCE_CTRL_SQUARE)) {
			SceUID fd = sceIoOpen(snapshot_path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
			sceIoWrite(fd, &snapshot, sizeof(ThreadOptimizerSnapshot));
			sceIoClose(fd);
			snapshot_changed = SCE_FALSE;
			showMessage("Snapshot has been saved.");
			cdlg_terminated = SCE_FALSE;
		}
		else if (ONPRESS(SCE_CTRL_TRIANGLE)) {
			showMessageYesNo("Do you want to delete this snapshot?");
			cdlg_delete_request = SCE_TRUE;
			cdlg_terminated = SCE_FALSE;
		}
	}

threadSelect_ret:

	if (selection == BACK_MAGIC)
		return BACK_MAGIC;
	else {
		selection_page = page;
		selection_page_mod = page_modifier;
		selection_thread = selection;
		return selection + page_modifier;
	}
}

unsigned int drawMainSelect(void)
{
	selection_page = 0;
	selection_page_mod = 0;
	selection_thread = 0;
	snapshot_loaded = SCE_FALSE;

	unsigned int preset_count = 0, total_pages = 0;

	SceUID dfd = sceIoDopen("ux0:data/ThreadOptimizer");
	if (dfd < 0) {
		sceIoMkdir("ux0:data/ThreadOptimizer", 0777);
		dfd = sceIoDopen("ux0:data/ThreadOptimizer");
	}

	SceIoDirent dirent;
	sceClibMemset(&preset_dir, 0, MAX_PRESET * 10);

	while (sceIoDread(dfd, &dirent) != 0) {
		sceClibStrncpy(preset_dir[preset_count], dirent.d_name, 10);
		preset_count++;
	}

	float total_pages_temp = (float)preset_count / (float)TOTAL_PER_PAGE;
	total_pages = ceilf(total_pages_temp);
	total_pages--;

	vita2d_sys_widget* widget[MAX_PRESET];

	int coordinate_mltp = 0;

	for (int i = 0; i < preset_count; i++) {
		widget[i] = vita2d_sys_create_widget_button(tex_button, font, 0, (TOPBAR_TEXT_Y + 22) + WIDGET_BUTTON_HEIGHT * coordinate_mltp, WIDGET_BUTTON_TEXT_DX, WIDGET_BUTTON_TEXT_DY, preset_dir[i]);
		coordinate_mltp++;
		if (coordinate_mltp == TOTAL_PER_PAGE)
			coordinate_mltp = 0;
	}

	unsigned int selection = selection_m, total_count = 0, page = selection_page_m, page_modifier = selection_page_mod_m;

	if (preset_count == 0) {
		showMessage("Error: There are no saved snapshots.");
		while (sceMsgDialogGetStatus() != 2)
			cdlgDraw();
		sceMsgDialogTerm();
		sceAppMgrDestroyAppByAppId(-2);
	}

	while (1) {
		vita2d_start_drawing();
		vita2d_clear_screen();

		vita2d_draw_texture(tex_bg, 0, 0);
		vita2d_pvf_draw_text(font, 480 - (vita2d_pvf_text_width(font, 1, "Snapshot Selection") / 2), TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "Snapshot Selection");
		if (page != 0)
			vita2d_pvf_draw_text(icon_font, LEFT_MARGIN, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "0");
		if (page != total_pages)
			vita2d_pvf_draw_text(icon_font, RIGHT_MARGIN, TOPBAR_TEXT_Y, RGBA8(255, 255, 255, 255), 1, "1");
		vita2d_draw_texture(tex_separator, 0, TOPBAR_TEXT_Y + 20);

		for (int i = 0; i < TOTAL_PER_PAGE && ((i + page_modifier) < preset_count); i++) {
			if (i == selection) {
				vita2d_sys_widget_set_highlight(widget[i + page_modifier], SCE_TRUE);
			}
			else
				vita2d_sys_widget_set_highlight(widget[i + page_modifier], SCE_FALSE);
			vita2d_sys_draw_widget(widget[i + page_modifier]);
		}

		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_end_shfb();

		if (ONPRESS(SCE_CTRL_DOWN)) {
			if ((selection != (TOTAL_PER_PAGE - 1)) && ((selection + page_modifier) != (preset_count - 1))) {
				vita2d_sys_widget_set_highlight_max();
				selection++;
			}
		}
		else if (ONPRESS(SCE_CTRL_UP)) {
			if (selection != 0) {
				vita2d_sys_widget_set_highlight_max();
				selection--;
			}
		}
		else if (ONPRESS(SCE_CTRL_R2)) {
			if (page != total_pages) {
				vita2d_sys_widget_set_highlight_max();
				page_modifier += TOTAL_PER_PAGE;
				selection = 0;
				page++;
			}
		}
		else if (ONPRESS(SCE_CTRL_L2)) {
			if (page != 0) {
				vita2d_sys_widget_set_highlight_max();
				page_modifier -= TOTAL_PER_PAGE;
				selection = 0;
				page--;
			}
		}
		else if (ONPRESS(SCE_CTRL_CROSS)) {
			for (int i = 0; i < preset_count; i++)
				vita2d_sys_delete_widget(widget[i]);
			break;
		}
	}

	selection_page_m = page;
	selection_page_mod_m = page_modifier;
	selection_m = selection;

	return selection + page_modifier;
}

int main(void)
{
	ScePVoid clibm_base;
	SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
	sceKernelGetMemBlockBase(clib_heap, &clibm_base);
	g_mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);

	vita2d_clib_pass_mspace(g_mspace);
	vita2d_init();

	vita2d_set_vblank_wait(1);
	vita2d_set_clear_color(RGBA8(105, 121, 125, 255));

	SceInt32 ret;
	SceUInt64 unk;

	vita2d_JPEG_decoder_initialize();
	tex_bg = vita2d_load_JPEG_file("app0:tex/bg.jpg", 0, 0, 0, 0, 0);
	vita2d_JPEG_decoder_finish();
	tex_separator = vita2d_load_GXT_file("app0:tex/texture.gxt", 0, 0);
	tex_button = vita2d_load_additional_GXT(tex_separator, 1);

	SceUID modid = _vshKernelSearchModuleByName("CoreUnlocker80000H", &unk);
	if (modid < 0) {
		showMessage("Error: CoreUnlocker80000H plugin not found.\n\nPlease install it and try again.");
		while (sceMsgDialogGetStatus() != 2)
			cdlgDraw();
		sceMsgDialogTerm();
		sceAppMgrDestroyAppByAppId(-2);
	}
	modid = _vshKernelSearchModuleByName("ioplus", &unk);
	if (modid < 0) {
		showMessage("Error: ioPlus plugin not found.\n\nPlease install it and try again.");
		while (sceMsgDialogGetStatus() != 2)
			cdlgDraw();
		sceMsgDialogTerm();
		sceAppMgrDestroyAppByAppId(-2);
	}

	vita2d_system_pvf_config configs[] = {
		{SCE_PVF_LANGUAGE_LATIN, SCE_PVF_FAMILY_SANSERIF, SCE_PVF_STYLE_REGULAR, NULL},
	};

	font = vita2d_load_system_pvf(1, configs, 14, 14);
	icon_font = vita2d_load_custom_pvf("sa0:data/font/pvf/psexchar.pvf", 14, 14);

	SceUID ctrl_thrd = sceKernelCreateThread("CtrlThread", controlsThread, 0x10000100, 0x1000, 0, 0, NULL);
	sceKernelStartThread(ctrl_thrd, 0, NULL);

	unsigned int selection;

repeat_0:

	selection = 0;
	selection = drawMainSelect();

repeat_1:

	selection = drawThreadSelect(selection);

	if (selection == BACK_MAGIC)
		goto repeat_0;

	selection = drawThreadEdit(selection);
	goto repeat_1;

	return 0;
}

