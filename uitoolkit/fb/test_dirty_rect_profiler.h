#ifndef DIRTY_RECT_PROFILER_H
#define DIRTY_RECT_PROFILER_H

#include <sys/types.h>

typedef enum {
  ID_PAGE_FLIP_FULL = 0,
  ID_PAGE_FLIP_WIDTH,
  ID_PAGE_FLIP_RECT,
  ID_PAGE_FLIP_TOTAL_BYTES,
  ID_RESTORE_HIDDEN_REGION,
  ID_DRAW_MOUSE_CURSOR,
  ID_UI_DISPLAY_PUT_IMAGE2,
  ID_UI_DISPLAY_FILL_WITH,
  ID_UI_DISPLAY_COPY_LINES2,
  ID_DIRTY_RECT_FUNCTION_MAX
} DRP_func_id_t;


/* --- global functions --- */

int dirty_rect_profiler_open(const char *filename);
void dirty_rect_profiler_collect(DRP_func_id_t id, u_int64_t bytes);
void dirty_rect_profiler_close(void);
#endif
