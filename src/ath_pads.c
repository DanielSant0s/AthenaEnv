#include "ath_env.h"
#include "include/pad.h"

#include <libds34bt.h>
#include <libds34usb.h>

duk_ret_t athena_gettype(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	int port = 0;
	if (argc == 1){
		port = duk_get_int(ctx, 0);
		if (port > 1) return duk_generic_error(ctx, "wrong port number.");
	}
	int mode = padInfoMode(port, 0, PAD_MODETABLE, 0);
	duk_push_int(ctx, mode);
	return 1;
}

duk_ret_t athena_getpad(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	int port = 0;
	if (argc == 1){
		port = duk_get_int(ctx, 0);
		if (port > 1) return duk_generic_error(ctx, "wrong port number.");
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

	duk_push_uint(ctx, paddata);
	return 1;
}

duk_ret_t athena_getleft(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1) return duk_generic_error(ctx, "wrong number of arguments.");
	int port = 0;
	if (argc == 1){
		port = duk_get_int(ctx, 0);
		if (port > 1) return duk_generic_error(ctx, "wrong port number.");
	}

	struct padButtonStatus buttons;

	int state = padGetState(port, 0);

	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) padRead(port, 0, &buttons);
	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) ds34bt_get_data(port, (u8 *)&buttons.btns);
	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) ds34usb_get_data(port, (u8 *)&buttons.btns);

	duk_push_int(ctx, buttons.ljoy_h-127);
	duk_push_int(ctx, buttons.ljoy_v-127);
	return 2;
	}

duk_ret_t athena_getright(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1) return duk_generic_error(ctx, "wrong number of arguments.");
	int port = 0;
	if (argc == 1){
		port = duk_get_int(ctx, 0);
		if (port > 1) return duk_generic_error(ctx, "wrong port number.");
	}

	struct padButtonStatus buttons;

	int state = padGetState(port, 0);

	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) padRead(port, 0, &buttons);
	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) ds34bt_get_data(port, (u8 *)&buttons.btns);
	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) ds34usb_get_data(port, (u8 *)&buttons.btns);

	duk_push_int(ctx, buttons.rjoy_h-127);
	duk_push_int(ctx, buttons.rjoy_v-127);
	return 2;
	}

duk_ret_t athena_getpressure(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1 && argc != 2) return duk_generic_error(ctx, "wrong number of arguments.");
	int port = 0;
	int button;
	if (argc == 2) {
		port = duk_get_int(ctx, 0);
		button = duk_get_int(ctx, 1);
	} else {
		button = duk_get_int(ctx, 0);
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

	duk_push_int(ctx, (uint32_t)pressure);
	return 1;
}

duk_ret_t athena_rumble(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2 && argc != 3) return duk_generic_error(ctx, "wrong number of arguments.");
	static char actAlign[6];
	int port = 0;
	if (argc == 3){
		port = duk_get_int(ctx, 0);
		actAlign[0] = duk_get_int(ctx, 1);
		actAlign[1] = duk_get_int(ctx, 2);
	} else {
		actAlign[0] = duk_get_int(ctx, 0);
		actAlign[1] = duk_get_int(ctx, 1);
	}

	int state = padGetState(port, 0);
	if ((state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1)) padSetActDirect(port, 0, actAlign);
	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) ds34bt_set_rumble(port, actAlign[1], actAlign[1]);
	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) ds34usb_set_rumble(port, actAlign[1], actAlign[1]);

	return 0;
}

duk_ret_t athena_check(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "wrong number of arguments.");
	int pad = duk_get_uint(ctx, 0);
	int button = duk_get_uint(ctx, 1);
	duk_push_boolean(ctx, (pad & button));
	return 1;
}


duk_ret_t athena_set_led(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3 && argc != 4) return duk_generic_error(ctx, "wrong number of arguments.");
	u8 led[4];
	int port = 0;
	if (argc == 4){
		port = duk_get_int(ctx, 0);
		led[0] = duk_get_int(ctx, 1);
		led[1] = duk_get_int(ctx, 2);
		led[2] = duk_get_int(ctx, 3);
	} else {
		led[0] = duk_get_int(ctx, 0);
		led[1] = duk_get_int(ctx, 1);
		led[2] = duk_get_int(ctx, 2);
	}

	led[3] = 0;

	if (ds34bt_get_status(port) & DS34BT_STATE_RUNNING) ds34bt_set_led(port, led);
	if (ds34usb_get_status(port) & DS34USB_STATE_RUNNING) ds34usb_set_led(port, led);

	return 0;
}

DUK_EXTERNAL duk_ret_t dukopen_pads(duk_context *ctx) {
    const duk_function_list_entry module_funcs[] = {
        { "get",                athena_getpad,       DUK_VARARGS },
        { "getLeftStick",       athena_getleft,      DUK_VARARGS },
        { "getRightStick",      athena_getright,     DUK_VARARGS },
        { "getType",            athena_gettype,      DUK_VARARGS },
        { "getPressure",        athena_getpressure,  DUK_VARARGS },
        { "rumble",             athena_rumble,       DUK_VARARGS },
        { "setLED",             athena_set_led,      DUK_VARARGS },
        { "check",              athena_check,        DUK_VARARGS },
        { NULL, NULL, 0 }
    };

    duk_push_object(ctx);  /* module result */
    duk_put_function_list(ctx, -1, module_funcs);

    return 1;  /* return module value */
}


void athena_pads_init(duk_context* ctx){
	push_athena_module(dukopen_pads,   		   "Pads");

	duk_push_uint(ctx, PAD_SELECT);
	duk_put_global_string(ctx, "PAD_SELECT");

	duk_push_uint(ctx, PAD_START);
	duk_put_global_string(ctx, "PAD_START");
	
	duk_push_uint(ctx, PAD_UP);
	duk_put_global_string(ctx, "PAD_UP");
	
	duk_push_uint(ctx, PAD_RIGHT);
	duk_put_global_string(ctx, "PAD_RIGHT");
	
	duk_push_uint(ctx, PAD_DOWN);
	duk_put_global_string(ctx, "PAD_DOWN");
	
	duk_push_uint(ctx, PAD_LEFT);
	duk_put_global_string(ctx, "PAD_LEFT");
	
	duk_push_uint(ctx, PAD_TRIANGLE);
	duk_put_global_string(ctx, "PAD_TRIANGLE");
	
	duk_push_uint(ctx, PAD_CIRCLE);
	duk_put_global_string(ctx, "PAD_CIRCLE");

	duk_push_uint(ctx, PAD_CROSS);
	duk_put_global_string(ctx, "PAD_CROSS");

	duk_push_uint(ctx, PAD_SQUARE);
	duk_put_global_string(ctx, "PAD_SQUARE");

	duk_push_uint(ctx, PAD_L1);
	duk_put_global_string(ctx, "PAD_L1");

	duk_push_uint(ctx, PAD_R1);
	duk_put_global_string(ctx, "PAD_R1");
	
	duk_push_uint(ctx, PAD_L2);
	duk_put_global_string(ctx, "PAD_L2");

	duk_push_uint(ctx, PAD_R2);
	duk_put_global_string(ctx, "PAD_R2");

	duk_push_uint(ctx, PAD_L3);
	duk_put_global_string(ctx, "PAD_L3");

	duk_push_uint(ctx, PAD_R3);
	duk_put_global_string(ctx, "PAD_R3");

	duk_push_uint(ctx, PAD_TYPE_DIGITAL);
	duk_put_global_string(ctx, "PAD_DIGITAL");

	duk_push_uint(ctx, PAD_TYPE_ANALOG);
	duk_put_global_string(ctx, "PAD_ANALOG");

	duk_push_uint(ctx, PAD_TYPE_DUALSHOCK);
	duk_put_global_string(ctx, "PAD_DUALSHOCK");

}
