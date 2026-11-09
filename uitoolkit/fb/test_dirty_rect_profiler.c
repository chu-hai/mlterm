#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#include <pobl/bl_conf_io.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h>

#include "test_dirty_rect_profiler.h"

#define FILE_HEADER1 "Timestamp           Function name          Count                Transfer bytes\n"
#define FILE_HEADER2 "------------------- ---------------------- -------- ---------------------------\n"
#define FILE_HEADER3 "Summary             Function name          Count                Transfer bytes\n"
#define FILE_HEADER4 "------------------- ---------------------- -------- ---------------------------\n"

static const char *DRP_func_names[ID_DIRTY_RECT_FUNCTION_MAX] = {
  "page_flip(Full Screen)",
  "page_flip(Full Width)",
  "page_flip(Dirty Rect)",
  "",
  "",
  "restore_hidden_region",
  "draw_mouse_cursor_line",
  "ui_display_put_image2",
  "ui_display_fill_with",
  "ui_display_copy_lines2"
};

typedef struct {
  FILE *fp;
  char *file_path;
  time_t last_sec;
  u_int64_t func_call_cnt[ID_DIRTY_RECT_FUNCTION_MAX];
  u_int64_t trans_bytes[3];
} DRP_data_t;

typedef struct {
  u_int64_t func_call_cnt[ID_DIRTY_RECT_FUNCTION_MAX];
  u_int64_t trans_bytes[3];
} DRP_summary_t;

static DRP_data_t DRP_data = {0};
static DRP_summary_t DRP_summary = {0};
static struct timespec Start_time, End_time;

/* --- static functions --- */

static void clear_DRP_data(void) {
  memset(DRP_data.func_call_cnt, 0, sizeof(DRP_data.func_call_cnt));
  memset(DRP_data.trans_bytes,   0, sizeof(DRP_data.trans_bytes));
}

static const char *DRP_get_function_name(DRP_func_id_t id) {
  if (id < 0 || id >= ID_DIRTY_RECT_FUNCTION_MAX) {
    bl_error_printf("Unknown function ID: %d\n", id);
    return "Unknown function ID";
  }
  return DRP_func_names[id];
}

static void DRP_flush(void) {
  time_t now = time(NULL);
  struct tm *tm;
  char timestamp[32];
  u_int64_t total_bytes;

  if ((tm = localtime(&now)) == NULL) {
    bl_error_printf("localtime() failed: %s\n", strerror(errno));
    return;
  }

  if (strftime(timestamp, sizeof(timestamp), "%Y/%m/%d %H:%M:%S", tm) == 0) {
    bl_error_printf("strftime() failed: %s\n", strerror(errno));
    return;
  }

  total_bytes = 0;
  for (int i = 0; i < ID_DIRTY_RECT_FUNCTION_MAX; i++) {
    if (i <= ID_PAGE_FLIP_RECT) {
      total_bytes += DRP_data.trans_bytes[i];
      fprintf(DRP_data.fp, "%19s %-22s %8lu %14lu (%7.3lfMiB)\n",
              (i == ID_PAGE_FLIP_FULL ? timestamp : ""),
              DRP_get_function_name((DRP_func_id_t)i),
              DRP_data.func_call_cnt[i],
              DRP_data.trans_bytes[i],
              ((double)DRP_data.trans_bytes[i] / 1048576));
    } else if (i == ID_PAGE_FLIP_TOTAL_BYTES) {
      fprintf(DRP_data.fp, "%19s %-22s %8s %14lu (%7.3lfMiB)\n",
              "",
              "",
              "",
              total_bytes,
              ((double)total_bytes / 1048576));
    } else if (i != ID_PAGE_FLIP_RECT_COUNT){
      if (DRP_data.func_call_cnt[i] > 0) {
        fprintf(DRP_data.fp, "%19s %-22s %8lu\n",
                "",
                DRP_get_function_name((DRP_func_id_t)i),
                DRP_data.func_call_cnt[i]);
      }
    }
  }
  fprintf(DRP_data.fp, "\n");
  fflush(DRP_data.fp);

  clear_DRP_data();
}

static void DRP_output_summary(void) {
  u_int64_t total_bytes;
  u_int64_t total_flip_count;
  u_int64_t total_marker_count;
  long elapsed_sec = BL_MAX(End_time.tv_sec - Start_time.tv_sec, 1);

  fprintf(DRP_data.fp, FILE_HEADER3);
  fprintf(DRP_data.fp, FILE_HEADER4);

  total_marker_count = 0;
  for (int i = ID_RESTORE_HIDDEN_REGION; i < ID_DIRTY_RECT_FUNCTION_MAX; i++) {
    total_marker_count += DRP_summary.func_call_cnt[i];
  }

  total_bytes = 0;
  total_flip_count = 0;
  for (int i = 0; i < ID_DIRTY_RECT_FUNCTION_MAX; i++) {
    if (i <= ID_PAGE_FLIP_RECT_COUNT) {
      total_flip_count += (i != ID_PAGE_FLIP_RECT) ? DRP_summary.func_call_cnt[i] : 0;
      total_bytes += DRP_summary.trans_bytes[i];
      fprintf(DRP_data.fp, "%-19s %-22s %8lu %14lu (%7.3lfMiB)\n",
              (i == ID_PAGE_FLIP_FULL ? "Page flip summary" : ""),
              DRP_get_function_name((DRP_func_id_t)i),
              DRP_summary.func_call_cnt[i],
              DRP_summary.trans_bytes[i],
              ((double)DRP_summary.trans_bytes[i] / 1048576));
    } else if (i == ID_PAGE_FLIP_TOTAL_BYTES) {
      fprintf(DRP_data.fp, "%-19s %-22s %8lu %14lu (%7.3lfMiB)\n",
              "",
              "Total",
              total_flip_count,
              total_bytes,
              ((double)total_bytes / 1048576));
      fprintf(DRP_data.fp, "%-19s %-22s %8.2lffps (%ldsec)\n\n",
              "",
              "",
              (double)total_flip_count / (elapsed_sec >= 1 ? elapsed_sec : 1),
              elapsed_sec);
    } else {
      if (DRP_summary.func_call_cnt[i] > 0) {
        fprintf(DRP_data.fp, "%-19s %-22s %8lu (%7.4lf%%)\n",
                (i == ID_RESTORE_HIDDEN_REGION ? "Marker summary" : ""),
                DRP_get_function_name((DRP_func_id_t)i),
                DRP_summary.func_call_cnt[i],
                (double)(DRP_summary.func_call_cnt[i]) * 100 / total_marker_count);
      } else {
        fprintf(DRP_data.fp, "%-19s %-22s %8lu (%7.4lf%%)\n",
                (i == ID_RESTORE_HIDDEN_REGION ? "Marker summary" : ""),
                DRP_get_function_name((DRP_func_id_t)i),
                DRP_summary.func_call_cnt[i],
                0.0);
      }
    }
  }
  fprintf(DRP_data.fp, "%19s %-22s %8lu\n",
          "",
          "Total",
          total_marker_count);
}

/* --- global functions --- */

int dirty_rect_profiler_open(const char *filename) {
  bl_debug_printf("DirtyRegion profiler has started.\n");
  if (DRP_data.fp != NULL) {
    bl_error_printf("Failed to open file: %s is already open.\n", filename);
    return -1;
  }

  memset(&DRP_data, 0, sizeof(DRP_data_t));
  DRP_data.file_path = bl_get_user_rc_path(filename);
  bl_debug_printf("DirtyRegion profiler logfile:[%s]\n", DRP_data.file_path);

#if 1
  if ((DRP_data.fp = fopen(DRP_data.file_path, "w")) == NULL) {
#else
  if ((DRP_data.fp = fopen(DRP_data.file_path, "a+")) == NULL) {
#endif
    bl_error_printf("Couldn't open %s.\n", DRP_data.file_path);
    return -1;
  }
  fprintf(DRP_data.fp, FILE_HEADER1);
  fprintf(DRP_data.fp, FILE_HEADER2);

  clock_gettime(CLOCK_MONOTONIC, &Start_time);

  return 0;
}

void dirty_rect_profiler_collect(DRP_func_id_t id, u_int64_t bytes) {
  time_t now = time(NULL);

  if (!DRP_data.fp || id < 0 || id >= ID_DIRTY_RECT_FUNCTION_MAX) {
    bl_error_printf("Invalid Argument or File is not ready.\n");
    return;
  }

  if (now != DRP_data.last_sec) {
    if (DRP_data.last_sec != 0) {
      DRP_flush();
    }
    DRP_data.last_sec = now;
  }

  DRP_data.func_call_cnt[id]++;
  DRP_summary.func_call_cnt[id]++;

  if (id <= ID_PAGE_FLIP_RECT) {
    DRP_data.trans_bytes[id] += bytes;
    DRP_summary.trans_bytes[id] += bytes;
  }
}

void dirty_rect_profiler_close(void) {
  if (!DRP_data.fp) {
    return;
  }

  clock_gettime(CLOCK_MONOTONIC, &End_time);

  DRP_flush();
  DRP_output_summary();
  fclose(DRP_data.fp);
  DRP_data.fp = NULL;
  bl_debug_printf("DirtyRegion profiler has finished.\n");
}

