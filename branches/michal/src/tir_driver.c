#include <stdlib.h>
#include <assert.h>
#include "tir.h"
#include "tir_driver.h"
#include "tir_img.h"
#include "tir_hw.h"
#include "list.h"
#include "cal.h"
#include <stdio.h>
#include <string.h>
#include "pref.h"
#include "pref_int.h"
#include "pref_global.h"
#include "runloop.h"
#include "image_process.h"
#include "usb_ifc.h"
#include "dyn_load.h"

pref_id min_blob = NULL;
pref_id max_blob = NULL;
pref_id threshold = NULL;
pref_id stat_bright = NULL;
pref_id ir_bright = NULL;
pref_id signals = NULL;
const char *storage_path = NULL;

bool threshold_changed = false;
bool status_brightness_changed = false;
bool ir_led_brightness_changed = false;
bool signal_flag = true;

init_usb_fun init_usb = NULL;
find_tir_fun find_tir = NULL;
prepare_device_fun prepare_device = NULL;
send_data_fun send_data = NULL;
receive_data_fun receive_data = NULL;
finish_usb_fun finish_usb = NULL;

static lib_fun_def_t functions[] = {
  {(char *)"init_usb", (void*) &init_usb},
  {(char *)"find_tir", (void*) &find_tir},
  {(char *)"prepare_device", (void*) &prepare_device},
  {(char *)"send_data", (void*) &send_data},
  {(char *)"receive_data", (void*) &receive_data},
  {(char *)"finish_usb", (void*) &finish_usb},
  {NULL, NULL}
};
static void *libhandle = NULL;


void flag_pref_changed(void *flag_ptr)
{
  *(bool*)flag_ptr = true;
}

int tir_get_prefs()
{
  char *dev_section = get_device_section();
  if(dev_section == NULL){
    return -1;
  }
  if(!open_pref(dev_section, "Max-blob", &max_blob)){
    return -1;
  }
  if(!open_pref(dev_section, "Min-blob", &min_blob)){
    return -1;
  }
  if(open_pref_w_callback(dev_section, "Status-led-brightness", &stat_bright,
                       flag_pref_changed, (void*)&status_brightness_changed)){
    status_brightness_changed = true;
  }
  if(open_pref_w_callback(dev_section, "Ir-led-brightness", &ir_bright,
                        flag_pref_changed, (void*)&ir_led_brightness_changed)){
    ir_led_brightness_changed = true;
  }
  if(open_pref_w_callback(dev_section, "Threshold", &threshold,
                        flag_pref_changed, (void*)&threshold_changed)){
    threshold_changed = true;
  }
  if(!open_pref(dev_section, "Status-signals", &signals)){
    return -1;
  }
  
  storage_path = get_storage_path();
  
  if(get_int(max_blob) == 0){
    log_message("Please set 'Max-blob' in section %s!\n", dev_section);
    if(!set_int(&max_blob, 200)){
      log_message("Can't set Max-blob!\n");
      return -1;
    }
  }
  if(get_int(min_blob) == 0){
    log_message("Please set 'Min-blob' in section %s!\n", dev_section);
    if(!set_int(&min_blob, 200)){
      log_message("Can't set Min-blob!\n");
      return -1;
    }
  }
  char *tmp = get_str(signals);
  if((tmp != NULL) && (strcasecmp(tmp, "off") == 0)){
    signal_flag = false;
  }
  return 0;
}

int tracker_init(struct camera_control_block *ccb)
{
  assert(ccb != NULL);
  assert((ccb->device.category == tir) || (ccb->device.category == tir_open));
  if((libhandle = lt_load_library((char *)"libltusb1.so", functions)) == NULL){
    return -1;
  }
  if(tir_get_prefs() != 0){
    return -1;
  }
  log_message("Lib loaded, prefs read...\n");
  if(open_tir(storage_path, false, !is_model_active())){
    float tf;
    get_res_tir(&(ccb->pixel_width), &(ccb->pixel_height), &tf);
    prepare_for_processing(ccb->pixel_width, ccb->pixel_height);
    return 0;
  }else{
    return -1;
  }
}

/*
static int tir_set_good(struct camera_control_block *ccb, bool arg)
{
  switch_green(arg);
  return 0;
}
*/

int tracker_get_frame(struct camera_control_block *ccb, struct frame_type *f)
{
  (void) ccb;
  unsigned int w,h;
  float hf;
  get_res_tir(&w, &h, &hf);
  
  f->width = w;
  f->height = h;
  image img = {
    .bitmap = f->bitmap,
    .w = w,
    .h = h,
    .ratio = hf
  };
  if(threshold_changed){
    threshold_changed = false;
    int new_threshold = get_int(threshold);
    if(new_threshold > 0){
      set_threshold(new_threshold);
    }
  }
  return read_blobs_tir(&(f->bloblist), get_int(min_blob), get_int(max_blob), &img);
}

int tracker_pause()
{
  return pause_tir() ? 0 : -1;
}

int tracker_resume()
{
  return resume_tir() ? 0 : -1;
}

int tracker_close()
{
  int res = close_tir() ? 0 : -1;;
  lt_unload_library(libhandle, functions);
  libhandle = NULL;
  return res;
}
