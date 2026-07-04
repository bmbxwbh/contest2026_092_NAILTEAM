#ifndef __SCENE_UI_H
#define __SCENE_UI_H

#include <nuttx/config.h>
#include <stdbool.h>
#include "display_manager.h"

bool scene_ui_init(void);
void scene_ui_build_primary(display_info_t *info);

#endif
