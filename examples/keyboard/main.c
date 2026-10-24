/*
 * Keyboard demo, in C: the port of main.js. A full custom keymap (identical
 * to the US layout baked into ps2kbd.irx), then raw mode, which reports
 * modifiers and every non-printable key by their USB HID usage codes.
 */

#include <string.h>

#include <athena/athena.h>
#include <ps2kbd.h>
#include "athenaos/app.h"

static const struct { uint8_t code; const char *name; } MODIFIERS[] = {
    { 0xE0, "Left Ctrl" }, { 0xE1, "Left Shift" }, { 0xE2, "Left Alt" }, { 0xE3, "Left GUI/Win" },
    { 0xE4, "Right Ctrl" }, { 0xE5, "Right Shift" }, { 0xE6, "Right Alt" }, { 0xE7, "Right GUI/Win" },
};

static int keyboard_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "Keyboard demo", "input",
                           "Plug a USB keyboard and press keys -- modifiers and arrows included", NULL, 0);
    AthenaPad *pad = athena_pad_open(0);

    athena_iop_load_module(athena_iop_get_module_by_name("ps2kbd"), 0, NULL);
    athena_keyboard_open();

    // --- Custom keymap test ---
    // PS2KbdSetKeymap() replaces the *entire* translation table at once (there's
    // no "patch a single key" mode), so to test it we build a full table here.
    // Codes follow the USB HID Keyboard/Keypad Usage Page (0x07); this table is
    // the default US QWERTY layout -- swap the assignments for another layout.
    static uint8_t keymap[PS2KBD_KEYMAP_SIZE], shift_keymap[PS2KBD_KEYMAP_SIZE];
    memset(keymap, 0, sizeof(keymap));
    memset(shift_keymap, 0, sizeof(shift_keymap));

    for (int i = 0; i < 26; i++) {
        keymap[0x04 + i] = 'a' + i;
        shift_keymap[0x04 + i] = 'A' + i;
    }
    const char *digits = "1234567890", *digits_shifted = "!@#$%^&*()";
    for (int i = 0; i < 10; i++) {
        keymap[0x1E + i] = digits[i];
        shift_keymap[0x1E + i] = digits_shifted[i];
    }
    static const struct { uint8_t code; char normal, shifted; } PUNCTUATION[] = {
        { 0x28, '\r', '\r' }, { 0x29, '\x1b', '\x1b' }, { 0x2A, '\b', '\b' }, { 0x2B, '\t', '\t' },
        { 0x2C, ' ', ' ' }, { 0x2D, '-', '_' }, { 0x2E, '=', '+' }, { 0x2F, '[', '{' }, { 0x30, ']', '}' },
        { 0x31, '\\', '|' }, { 0x33, ';', ':' }, { 0x34, '\'', '"' }, { 0x35, '`', '~' }, { 0x36, ',', '<' },
        { 0x37, '.', '>' }, { 0x38, '/', '?' },
    };
    for (size_t i = 0; i < sizeof(PUNCTUATION) / sizeof(PUNCTUATION[0]); i++) {
        keymap[PUNCTUATION[i].code] = PUNCTUATION[i].normal;
        shift_keymap[PUNCTUATION[i].code] = PUNCTUATION[i].shifted;
    }

    athena_keyboard_set_keymap(keymap, shift_keymap, NULL);
    athenaos_terminal_log(&term, "Custom keymap applied.");

    // --- Raw mode: modifiers and other non-printable keys ---
    // In normal ("cooked") mode only translated ASCII comes through, so pure
    // modifier keys and non-printable keys (arrows, F1-F12, ...) are dropped.
    // Raw mode reports every key as a code + pressed/released event instead.
    athena_keyboard_set_read_mode(PS2KBD_READMODE_RAW);

    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;

        uint8_t state, code;
        while (athena_keyboard_read_raw(&state, &code) > 0) {
            const bool pressed = state == PS2KBD_RAWKEY_DOWN;
            const char *name = NULL;
            for (size_t i = 0; i < sizeof(MODIFIERS) / sizeof(MODIFIERS[0]); i++)
                if (MODIFIERS[i].code == code)
                    name = MODIFIERS[i].name;
            if (name)
                athenaos_terminal_log(&term, "%s %s", name, pressed ? "pressed" : "released");
            else if (pressed)
                athenaos_terminal_log(&term, "raw key 0x%x pressed", code);
        }

        athenaos_terminal_draw(&term);
    }

    athena_keyboard_deinit();
    return 0;
}

ATHENAOS_APP(keyboard, keyboard_main)
