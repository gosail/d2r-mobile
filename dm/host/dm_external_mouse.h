#pragma once
#include <stdint.h>
// Physical mouse events. All coordinates are normalised; UI uses the 16:9
// interface rectangle and world uses the actual rendered scene.
enum { DM_MOUSE_DOWN, DM_MOUSE_DRAG, DM_MOUSE_UP, DM_MOUSE_HOVER,
       DM_MOUSE_LEAVE, DM_MOUSE_RIGHT_DOWN, DM_MOUSE_RIGHT_UP, DM_MOUSE_WHEEL,
       DM_MOUSE_HOVER_END, DM_MOUSE_MIDDLE_DOWN, DM_MOUSE_MIDDLE_UP }; // Hover recognizer ended; preserve mouse button/drag state.
// May be called on the platform/UI thread; never touches game widgets.
void dm_external_mouse_event(int op, float ux, float uy, float wx, float wy, int arg);
// Both functions below run only on the guest controller-poll thread.
void dm_external_mouse_step(void);
void dm_external_mouse_apply(uint64_t *buttons, int32_t *rx, int32_t *ry);
int dm_external_mouse_active(void);
