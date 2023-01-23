#include "ath_env.h"
#include "include/pad.h"

#include <libds34bt.h>
#include <libds34usb.h>

static JSValue athena_gettype(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0 && argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int port = 0;
	if (argc == 1){
		JS_ToInt32(ctx, &port, argv[0]);
		if (port > 1) return JS_ThrowSyntaxError(ctx, "wrong port number.");
	}
	int mode = padInfoMode(port, 0, PAD_MODETABLE, 0);
	return JS_NewInt32(ctx, mode);
}

static JSValue athena_getpad(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0 && argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int port = 0;
	if (argc == 1){
		JS_ToInt32(ctx, &port, argv[0]);
		if (port > 1) return JS_ThrowSyntaxError(ctx, "wrong port number.");
	}

	struct padButtonStatus buttons;
	u32 paddata = 0;
	int ret;

	int state = padGetState(port, 0);

	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) {
        // pad is connected. Read pad button information.
        ret = padRead(port, 0, &buttons); // port, slot, buttons
        if (ret != 0) {
            paddata = 0xffff ^ buttons.btns;
        }
    } 

	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) {
        ret = ds34bt_get_data(port, (u8 *)&buttons.btns);
        if (ret != 0) {
            paddata |= 0xffff ^ buttons.btns;
        }
    }

	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) {
        ret = ds34usb_get_data(port, (u8 *)&buttons.btns);
        if (ret != 0) {
            paddata |= 0xffff ^ buttons.btns;
        }
    }

    JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "btns", JS_NewUint32(ctx, paddata), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "lx", JS_NewInt32(ctx, buttons.ljoy_h-127), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "ly", JS_NewInt32(ctx, buttons.ljoy_v-127), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "rx", JS_NewInt32(ctx, buttons.rjoy_h-127), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "ry", JS_NewInt32(ctx, buttons.rjoy_v-127), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_getpressure(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1 && argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	int port = 0;
	int button;
	if (argc == 2) {
		JS_ToInt32(ctx, &port, argv[0]);
		JS_ToInt32(ctx, &button, argv[1]);
	} else {
		JS_ToInt32(ctx, &button, argv[0]);
	}
	
	struct padButtonStatus pad;

	int pressure = 255;

	int state = padGetState(port, 0);

	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) padRead(port, 0, &pad);
	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) ds34bt_get_data(port, (u8 *)&pad.btns);
	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) ds34usb_get_data(port, (u8 *)&pad.btns);

	switch (button) {
	    case PAD_RIGHT:
	        pressure = pad.right_p;
	        break;
	    case PAD_LEFT:
	        pressure = pad.left_p;
	        break;
	    case PAD_UP:
	        pressure = pad.up_p;
	        break;
		case PAD_DOWN:
	        pressure = pad.down_p;
	        break;
		case PAD_TRIANGLE:
	        pressure = pad.triangle_p;
	        break;
		case PAD_CIRCLE:
	        pressure = pad.circle_p;
	        break;
		case PAD_CROSS:
	        pressure = pad.cross_p;
	        break;
		case PAD_SQUARE:
	        pressure = pad.square_p;
	        break;
		case PAD_L1:
	        pressure = pad.l1_p;
	        break;
		case PAD_R1:
	        pressure = pad.r1_p;
	        break;
		case PAD_L2:
	        pressure = pad.l2_p;
	        break;
		case PAD_R2:
	        pressure = pad.r2_p;
	        break;
	    default:
	        pressure = 0;
	        break;
    }

	JS_NewInt32(ctx, pressure);
}

static JSValue athena_rumble(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 2 && argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	static char actAlign[6];
	int port = 0;
	if (argc == 3){
		JS_ToInt32(ctx, &port, argv[0]);
		JS_ToInt32(ctx, &actAlign[0], argv[1]);
		JS_ToInt32(ctx, &actAlign[1], argv[2]);
	} else {
		JS_ToInt32(ctx, &actAlign[0], argv[0]);
		JS_ToInt32(ctx, &actAlign[1], argv[1]);
	}

	int state = padGetState(port, 0);
	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) padSetActDirect(port, 0, actAlign);
	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) ds34bt_set_rumble(port, actAlign[1], actAlign[1]);
	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) ds34usb_set_rumble(port, actAlign[1], actAlign[1]);

	return 0;
}

static JSValue athena_check(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	int pad, button;
    JSValue val;

    val = JS_GetPropertyStr(ctx, argv[0], "btns");
	JS_ToUint32(ctx, &pad, val);

	JS_ToInt32(ctx, &button, argv[1]);

	return JS_NewBool(ctx, (pad & button));
}


static JSValue athena_set_led(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3 && argc != 4) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	u8 led[4];
	int port = 0;
	if (argc == 4){
		JS_ToInt32(ctx, &port, argv[0]);
		JS_ToInt32(ctx, &led[0], argv[1]);
		JS_ToInt32(ctx, &led[1], argv[2]);
		JS_ToInt32(ctx, &led[2], argv[3]);
	} else {
		JS_ToInt32(ctx, &led[0], argv[0]);
		JS_ToInt32(ctx, &led[1], argv[1]);
		JS_ToInt32(ctx, &led[2], argv[2]);
	}

	led[3] = 0;

	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) ds34bt_set_led(port, led);
	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) ds34usb_set_led(port, led);

	return 0;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("get", 1, athena_getpad),
    JS_CFUNC_DEF("getType", 1, athena_gettype),
    JS_CFUNC_DEF("getPressure", 2, athena_getpressure),
    JS_CFUNC_DEF("rumble", 3, athena_rumble),
    JS_CFUNC_DEF("setLED", 4, athena_set_led),
    JS_CFUNC_DEF("check", 2, athena_check),
	JS_PROP_INT32_DEF("SELECT", PAD_SELECT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("START", PAD_START, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("UP", PAD_UP, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("RIGHT", PAD_RIGHT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("DOWN", PAD_DOWN, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("LEFT", PAD_LEFT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TRIANGLE", PAD_TRIANGLE, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("CIRCLE", PAD_CIRCLE, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("CROSS", PAD_CROSS, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("SQUARE", PAD_SQUARE, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("L1", PAD_L1, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("R1", PAD_R1, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("L2", PAD_L2, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("R2", PAD_R2, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("L3", PAD_L3, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("R3", PAD_R3, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("DIGITAL", PAD_TYPE_DIGITAL, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("ANALOG", PAD_TYPE_ANALOG, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("DUALSHOCK", PAD_TYPE_DUALSHOCK, JS_PROP_CONFIGURABLE ),

};



static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_pads_init(JSContext* ctx){
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Pads");
}
