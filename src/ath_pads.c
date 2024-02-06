#include "ath_env.h"
#include "include/pad.h"

#include <libds34bt.h>
#include <libds34usb.h>

static JSClassID js_pads_class_id;

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

static JSValue athena_new_pad_event(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int buttons, flavour;
	JS_ToInt32(ctx, &buttons, argv[0]);
	JS_ToInt32(ctx, &flavour, argv[1]);
	int id = js_new_input_event(buttons, JS_DupValue(ctx, argv[2]), flavour);
	return JS_NewInt32(ctx, id);
}

static JSValue athena_delete_pad_event(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int id;
	JS_ToInt32(ctx, &id, argv[0]);
	js_delete_input_event(id);
	return JS_UNDEFINED;
}

static JSValue athena_getstate(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0 && argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int port = 0;
	if (argc == 1){
		JS_ToInt32(ctx, &port, argv[0]);
		if (port > 1) return JS_ThrowSyntaxError(ctx, "wrong port number.");
	}
	int mode = padGetState(port, 0);
	return JS_NewInt32(ctx, mode);
}

static bool pad1_initialized = false;

static JSValue athena_getpad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv){
	int port = 0;

    JSPads* pad;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    obj = JS_NewObjectClass(ctx, js_pads_class_id);
	if (JS_IsException(obj))
        goto fail;
	pad = js_mallocz(ctx, sizeof(*pad));
    if (!pad)
        return JS_EXCEPTION;
	if (argc == 1){
		JS_ToInt32(ctx, &port, argv[0]);
		if (port == 1 && !pad1_initialized) {
			initializePad(1, 0);
			pad1_initialized = true;
		}
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

	pad->btns = paddata;
	pad->lx = buttons.ljoy_h-127;
	pad->ly = buttons.ljoy_v-127;
	pad->rx = buttons.rjoy_h-127;
	pad->ry = buttons.rjoy_v-127;
	pad->port = port;

    JS_SetOpaque(obj, pad);
    return obj;

 fail:
    js_free(ctx, pad);
    return JS_EXCEPTION;
}

void js_pads_update(JSPads *pad) {
	struct padButtonStatus buttons;
	u32 paddata = 0;
	int ret;

	int state = padGetState(pad->port, 0);

	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) {
        // pad is connected. Read pad button information.
        ret = padRead(pad->port, 0, &buttons); // port, slot, buttons
        if (ret != 0) {
            paddata = 0xffff ^ buttons.btns;
        }
    } 

	if (ds34bt_get_status(pad->port) & DS34BT_STATE_RUNNING) {
        ret = ds34bt_get_data(pad->port, (u8 *)&buttons.btns);
        if (ret != 0) {
            paddata |= 0xffff ^ buttons.btns;
        }
    }

	if (ds34usb_get_status(pad->port) & DS34USB_STATE_RUNNING) {
        ret = ds34usb_get_data(pad->port, (u8 *)&buttons.btns);
        if (ret != 0) {
            paddata |= 0xffff ^ buttons.btns;
        }
    }

	pad->old_btns = pad->btns;
	pad->old_lx = pad->lx; 
	pad->old_ly = pad->ly; 
	pad->old_rx = pad->rx; 
	pad->old_ry = pad->ry; 

	pad->btns = paddata;
	pad->lx = buttons.ljoy_h-127;
	pad->ly = buttons.ljoy_v-127;
	pad->rx = buttons.rjoy_h-127;
	pad->ry = buttons.rjoy_v-127;
}

static JSValue athena_update(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    JSPads *pad = JS_GetOpaque2(ctx, this_val, js_pads_class_id);

	struct padButtonStatus buttons;
	u32 paddata = 0;
	int ret;

	int state = padGetState(pad->port, 0);

	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) {
        // pad is connected. Read pad button information.
        ret = padRead(pad->port, 0, &buttons); // port, slot, buttons
        if (ret != 0) {
            paddata = 0xffff ^ buttons.btns;
        }
    } 

	if (ds34bt_get_status(pad->port) & DS34BT_STATE_RUNNING) {
        ret = ds34bt_get_data(pad->port, (u8 *)&buttons.btns);
        if (ret != 0) {
            paddata |= 0xffff ^ buttons.btns;
        }
    }

	if (ds34usb_get_status(pad->port) & DS34USB_STATE_RUNNING) {
        ret = ds34usb_get_data(pad->port, (u8 *)&buttons.btns);
        if (ret != 0) {
            paddata |= 0xffff ^ buttons.btns;
        }
    }

	pad->old_btns = pad->btns;
	pad->old_lx = pad->lx; 
	pad->old_ly = pad->ly; 
	pad->old_rx = pad->rx; 
	pad->old_ry = pad->ry; 

	pad->btns = paddata;
	pad->lx = buttons.ljoy_h-127;
	pad->ly = buttons.ljoy_v-127;
	pad->rx = buttons.rjoy_h-127;
	pad->ry = buttons.rjoy_v-127;

    return JS_UNDEFINED;
}

static JSValue athena_getpressure(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1 && argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	int port = 0;
	u32 button;
	if (argc == 2) {
		JS_ToInt32(ctx, &port, argv[0]);
		JS_ToUint32(ctx, &button, argv[1]);
	} else {
		JS_ToUint32(ctx, &button, argv[0]);
	}
	
	struct padButtonStatus pad;

	unsigned char pressure = 255;

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

	return JS_NewInt32(ctx, pressure);
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

	return JS_UNDEFINED;
}

static JSValue athena_check(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int button;
	JSPads *pad = JS_GetOpaque2(ctx, this_val, js_pads_class_id);

	JS_ToInt32(ctx, &button, argv[0]);

	return JS_NewBool(ctx, (pad->btns & button));
}

static JSValue athena_justpressed(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int button;
	JSPads *pad = JS_GetOpaque2(ctx, this_val, js_pads_class_id);

	JS_ToInt32(ctx, &button, argv[0]);

	return JS_NewBool(ctx, (pad->btns & button) && !(pad->old_btns & button));
}

static JSValue athena_seteventhandler(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int button;
	JSPads *pad = JS_GetOpaque2(ctx, this_val, js_pads_class_id);
	js_set_input_event_handler(pad);
	return JS_UNDEFINED;
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

	return JS_UNDEFINED;
}

static JSValue js_pad_get_prop(JSContext *ctx, JSValueConst this_val, int magic)
{
	JSValue val = JS_UNDEFINED;
    JSPads *pad = JS_GetOpaque2(ctx, this_val, js_pads_class_id);
	
    if (!pad){
		return JS_EXCEPTION;
	}

	switch(magic) {
		case 0:
			val = JS_NewUint32(ctx, pad->btns);
			break;
		case 1:
			val = JS_NewInt32(ctx, pad->lx);
			break;
		case 2:
			val = JS_NewInt32(ctx, pad->ly);
			break;
		case 3:
			val = JS_NewInt32(ctx, pad->rx);
			break;
		case 4:
			val = JS_NewInt32(ctx, pad->ry);
			break;
		case 5:
			val = JS_NewUint32(ctx, pad->old_btns);
			break;
		case 6:
			val = JS_NewInt32(ctx, pad->old_lx);
			break;
		case 7:
			val = JS_NewInt32(ctx, pad->old_ly);
			break;
		case 8:
			val = JS_NewInt32(ctx, pad->old_rx);
			break;
		case 9:
			val = JS_NewInt32(ctx, pad->old_ry);
			break;
	}

	return val;

}

static JSValue js_pad_set_prop(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSPads *pad = JS_GetOpaque2(ctx, this_val, js_pads_class_id);
    u32 v;

    if (!pad || JS_ToUint32(ctx, &v, val)){
		return JS_EXCEPTION;
	}

	switch(magic) {
		case 0:
			pad->btns = v;
			break;
		case 1:
			pad->lx = v;
			break;
		case 2:
			pad->ly = v;
			break;
		case 3:
			pad->rx = v;
			break;
		case 4:
			pad->ry = v;
			break;
		case 5:
			pad->old_btns = v;
			break;
		case 6:
			pad->old_lx = v;
			break;
		case 7:
			pad->old_ly = v;
			break;
		case 8:
			pad->old_rx = v;
			break;
		case 9:
			pad->old_ry = v;
			break;
	}

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("get", 1, athena_getpad),
    JS_CFUNC_DEF("getType", 1, athena_gettype),
	JS_CFUNC_DEF("getState", 1, athena_getstate),
    JS_CFUNC_DEF("getPressure", 2, athena_getpressure),
    JS_CFUNC_DEF("rumble", 3, athena_rumble),
    JS_CFUNC_DEF("setLED", 4, athena_set_led),
	
	JS_CFUNC_DEF("newEvent", 3, athena_new_pad_event),
	JS_CFUNC_DEF("deleteEvent", 1, athena_delete_pad_event),

	JS_PROP_INT32_DEF("PRESSED", PRESSED_EVENT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("JUST_PRESSED", JUSTPRESSED_EVENT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("NONPRESSED", NONPRESSED_EVENT, JS_PROP_CONFIGURABLE ),

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

	JS_PROP_INT32_DEF("TYPE_NEJICON", PAD_TYPE_NEJICON, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_KONAMIGUN", PAD_TYPE_KONAMIGUN, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_DIGITAL", PAD_TYPE_DIGITAL, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_ANALOG", PAD_TYPE_ANALOG, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_NAMCOGUN", PAD_TYPE_NAMCOGUN, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_DUALSHOCK", PAD_TYPE_DUALSHOCK, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_JOGCON", PAD_TYPE_JOGCON, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_EX_TSURICON", PAD_TYPE_EX_TSURICON, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("TYPE_EX_JOGCON", PAD_TYPE_EX_JOGCON, JS_PROP_CONFIGURABLE ),

	JS_PROP_INT32_DEF("STATE_DISCONN", PAD_STATE_DISCONN,  JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("STATE_FINDPAD", PAD_STATE_FINDPAD,  JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("STATE_FINDCTP1", PAD_STATE_FINDCTP1, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("STATE_EXECCMD", PAD_STATE_EXECCMD,  JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("STATE_STABLE", PAD_STATE_STABLE,   JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("STATE_ERROR", PAD_STATE_ERROR,    JS_PROP_CONFIGURABLE ),
};

static const JSCFunctionListEntry js_pad_proto_funcs[] = {
    JS_CFUNC_DEF("update", 0, athena_update),
	JS_CFUNC_DEF("pressed", 1, athena_check),
	JS_CFUNC_DEF("justPressed", 1, athena_justpressed),
	JS_CFUNC_DEF("setEventHandler", 0, athena_seteventhandler),
	JS_CGETSET_MAGIC_DEF("btns", js_pad_get_prop, js_pad_set_prop, 0),
	JS_CGETSET_MAGIC_DEF("lx",   js_pad_get_prop, js_pad_set_prop, 1),
	JS_CGETSET_MAGIC_DEF("ly",   js_pad_get_prop, js_pad_set_prop, 2),
	JS_CGETSET_MAGIC_DEF("rx",   js_pad_get_prop, js_pad_set_prop, 3),
	JS_CGETSET_MAGIC_DEF("ry",   js_pad_get_prop, js_pad_set_prop, 4),
	JS_CGETSET_MAGIC_DEF("old_btns", js_pad_get_prop, js_pad_set_prop, 5),
	JS_CGETSET_MAGIC_DEF("old_lx",   js_pad_get_prop, js_pad_set_prop, 6),
	JS_CGETSET_MAGIC_DEF("old_ly",   js_pad_get_prop, js_pad_set_prop, 7),
	JS_CGETSET_MAGIC_DEF("old_rx",   js_pad_get_prop, js_pad_set_prop, 8),
	JS_CGETSET_MAGIC_DEF("old_ry",   js_pad_get_prop, js_pad_set_prop, 9),
};

static JSClassDef js_pads_class = {
    "Pad",
    //.finalizer = js_std_file_finalizer,
}; 

static int js_pads_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue proto;

    /* the class ID is created once */
    JS_NewClassID(&js_pads_class_id);
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), js_pads_class_id, &js_pads_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_pad_proto_funcs,
                               countof(js_pad_proto_funcs));
    JS_SetClassProto(ctx, js_pads_class_id, proto);

    return JS_SetModuleExportList(ctx, m, module_funcs,
                           countof(module_funcs));
;
}

JSModuleDef *athena_pads_init(JSContext* ctx){
    return athena_push_module(ctx, js_pads_init, module_funcs, countof(module_funcs), "Pads");
}
