#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ath_env.h>

#ifdef ATHENA_BOX2D
#include <box2d/box2d.h>
#include "ath_box2d.h"

void js_b2_userdata_link(JSB2World *world, JSB2UserDataBox *box) {
    box->world = world;
    box->prev = NULL;
    box->next = (JSB2UserDataBox *)world->userDataList;
    if (world->userDataList) ((JSB2UserDataBox *)world->userDataList)->prev = box;
    world->userDataList = box;
}

static void js_b2_userdata_unlink(JSB2UserDataBox *box) {
    if (!box->world) return;
    if (box->prev) box->prev->next = box->next;
    else box->world->userDataList = box->next;
    if (box->next) box->next->prev = box->prev;
    box->world = NULL;
}

void *js_b2_box_userdata(JSContext *ctx, JSB2World *world, JSValueConst value) {
    JSB2UserDataBox *box = (JSB2UserDataBox *)malloc(sizeof(JSB2UserDataBox));
    if (!box) return NULL;
    box->rt = JS_GetRuntime(ctx);
    box->value = JS_DupValue(ctx, value);
    box->world = NULL;
    box->prev = box->next = NULL;
    if (world) js_b2_userdata_link(world, box);
    return box;
}

void js_b2_free_userdata_box(void *ptr) {
    JSB2UserDataBox *box = (JSB2UserDataBox *)ptr;
    if (box) {
        js_b2_userdata_unlink(box);
        JS_FreeValueRT(box->rt, box->value);
        free(box);
    }
}

JSValue js_b2_get_userdata_box(JSContext *ctx, void *ptr) {
    JSB2UserDataBox *box = (JSB2UserDataBox *)ptr;
    if (!box) return JS_UNDEFINED;
    return JS_DupValue(ctx, box->value);
}


#endif