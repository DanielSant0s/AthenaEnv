#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "ath_env.h"
#include <ps2cam_rpc.h>

//int PS2CamGetDeviceInfo(int handle, PS2CAM_DEVICE_INFO *info);
//int PS2CamExtractFrame(int handle, char *buffer, int bufsize);

static JSValue athena_camerainit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int mode = 0;
  	if (argc == 1) JS_ToInt32(ctx, &mode, argv[0]);
	PS2CamInit(mode);
	return JS_UNDEFINED;
}

static JSValue athena_cameracount(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	return JS_NewInt32(ctx, PS2CamGetDeviceCount());
}

static JSValue athena_cameraopen(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int index = 0;
  	if (argc == 1) JS_ToInt32(ctx, &index, argv[0]);
	return JS_NewInt32(ctx, PS2CamOpenDevice(index));
}

static JSValue athena_cameraclose(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int devid;
  	JS_ToInt32(ctx, &devid, argv[0]);
	return JS_NewInt32(ctx, PS2CamCloseDevice(devid));
}

static JSValue athena_camerastatus(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int devid;
  	JS_ToInt32(ctx, &devid, argv[0]);
	return JS_NewInt32(ctx, PS2CamGetDeviceStatus(devid));
}

static JSValue athena_camerabandwidth(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int devid, bandwidth;
  	JS_ToInt32(ctx, &devid, argv[0]);
	JS_ToInt32(ctx, &bandwidth, argv[1]);
	return JS_NewInt32(ctx, PS2CamSetDeviceBandwidth(devid, bandwidth));
}

static JSValue athena_camerapacket(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int devid;
  	JS_ToInt32(ctx, &devid, argv[0]);
	return JS_NewInt32(ctx, PS2CamReadPacket(devid));
}

static JSValue athena_cameraled(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int devid, mode;
  	JS_ToInt32(ctx, &devid, argv[0]);
	JS_ToInt32(ctx, &mode, argv[1]);
	return JS_NewInt32(ctx, PS2CamSetLEDMode(devid, mode));
}

static JSValue athena_setconfig(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int devid;
	PS2CAM_DEVICE_CONFIG	cfg;

	cfg.ssize = sizeof(cfg);
	cfg.mask	= CAM_CONFIG_MASK_DIMENSION|CAM_CONFIG_MASK_DIVIDER|CAM_CONFIG_MASK_OFFSET|CAM_CONFIG_MASK_FRAMERATE;
	cfg.width	= 640;
	cfg.height	= 480;
	cfg.x_offset =0x00;
	cfg.y_offset =0x00;
	cfg.h_divider =0;
	cfg.v_divider =0;
	cfg.framerate =30;

	JS_ToInt32(ctx, &devid, argv[0]);

	if(argc == 3) {
		JS_ToInt32(ctx, &cfg.width, argv[1]);
		JS_ToInt32(ctx, &cfg.height, argv[2]);
	}

	if(argc == 5) {
		JS_ToInt32(ctx, &cfg.x_offset, argv[3]);
		JS_ToInt32(ctx, &cfg.y_offset, argv[4]);
	}

	if(argc == 6) {
		JS_ToInt32(ctx, &cfg.framerate, argv[5]);
	}

	if(argc == 8) {
		JS_ToInt32(ctx, &cfg.h_divider, argv[6]);
		JS_ToInt32(ctx, &cfg.v_divider, argv[7]);
	}

	PS2CamSetDeviceConfig(devid, &cfg);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("init", 1, athena_camerainit),
    JS_CFUNC_DEF("count", 0, athena_cameracount),
	JS_CFUNC_DEF("open", 1, athena_cameraopen),
	JS_CFUNC_DEF("close", 1, athena_cameraclose),
    JS_CFUNC_DEF("getStatus", 1, athena_camerastatus),
	JS_CFUNC_DEF("setBandwidth", 2, athena_camerabandwidth),
    JS_CFUNC_DEF("readPacket", 1, athena_camerapacket),
	JS_CFUNC_DEF("setLED", 2, athena_cameraled),
	JS_CFUNC_DEF("setConfig", 8, athena_setconfig),
};

static int camera_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_camera_init(JSContext* ctx){
	return athena_push_module(ctx, camera_init, module_funcs, countof(module_funcs), "Camera");
}