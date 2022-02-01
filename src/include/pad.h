#include <libpad.h>

extern int waitPadReady(int port, int slot);
extern int isButtonPressed(u32 button);
extern int initializePad(int port, int slot);
extern void pad_init();
extern struct padButtonStatus readPad(int port, int slot);