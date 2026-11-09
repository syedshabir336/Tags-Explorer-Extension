#ifndef TAG_EXPLORER_ICONS_H
#define TAG_EXPLORER_ICONS_H
#include <windows.h>
int icons_init(void);
void icons_shutdown(void);
BOOL icons_draw_button(const DRAWITEMSTRUCT *draw);
#endif
