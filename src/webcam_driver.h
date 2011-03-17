#ifndef WEBCAM_DRIVER__H
#define WEBCAM_DRIVER__H

#include <linux/types.h>
#include "cal.h"

#ifdef __cplusplus
extern "C" {
#endif

int ltr_int_tracker_init(struct camera_control_block *ccb);
int ltr_int_tracker_shutdown();
int ltr_int_tracker_suspend();
int ltr_int_tracker_wakeup();
int ltr_int_tracker_get_frame(struct camera_control_block *ccb, struct frame_type *f);

extern dev_interface ltr_int_webcam_interface;


typedef struct{
  int i; //index into pixel format table
  __u32 fourcc;
  int w;
  int h;
  int fps_num, fps_den; 
} webcam_format;

typedef struct{
  char **fmt_strings; //Format table
  webcam_format *formats;
  int entries;
} webcam_formats;

int ltr_int_enum_webcams(char **ids[]);
int ltr_int_enum_webcam_formats(char *id, webcam_formats *formats);
int ltr_int_enum_webcam_formats_cleanup(webcam_formats *all_formats);

#ifdef __cplusplus
}
#endif

#endif
