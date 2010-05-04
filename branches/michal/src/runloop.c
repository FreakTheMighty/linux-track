#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "cal.h"
#include "utils.h"
#include "runloop.h"

static enum request_t {CONTINUE, RUN, PAUSE, SHUTDOWN} request;
static enum cal_device_state_type tracker_state = STOPPED;
static pthread_cond_t state_cv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t state_mx = PTHREAD_MUTEX_INITIALIZER;

int ltr_cal_run(struct camera_control_block *ccb, frame_callback_fun cbk)
{
  assert(ccb != NULL);
  assert(cbk != NULL);
  int retval;
  enum request_t my_request;
  struct frame_type frame;
  bool stop_flag = false;
  
  if(tracker_init(ccb) != 0){
    return -1;
  }
  frame.bloblist.blobs = my_malloc(sizeof(struct blob_type) * 3);
  frame.bloblist.num_blobs = 3;
  frame.bitmap = NULL;
  
  tracker_state = RUNNING;
  while(1){
    pthread_mutex_lock(&state_mx);
    my_request = request;
    request = CONTINUE;
    pthread_mutex_unlock(&state_mx);
    switch(tracker_state){
      case RUNNING:
        switch(my_request){
          case PAUSE:
            tracker_state = PAUSED;
            tracker_pause();
            break;
          case SHUTDOWN:
            stop_flag = true;
            break;
          default:
            retval = tracker_get_frame(ccb, &frame);
            if((retval == -1) || (cbk(ccb, &frame) < 0)){
              stop_flag = true;
            }
            break;
        }
        break;
      case PAUSED:
        pthread_mutex_lock(&state_mx);
        while((request == PAUSE) || (request == CONTINUE)){
          pthread_cond_wait(&state_cv, &state_mx);
        }
        my_request = request;
        request = CONTINUE;
        pthread_mutex_unlock(&state_mx);
        tracker_resume();
        switch(my_request){
          case RUN:
            tracker_state = RUNNING;
            break;
          case SHUTDOWN:
            stop_flag = true;
            break;
          default:
            assert(0);
            break;
        }
        break;
      default:
        assert(0);
        break;
    }
    if(stop_flag == true){
      break;
    }
  }
  
  tracker_close();
  frame_free(ccb, &frame);
  return 0;
}

int signal_request(enum request_t req)
{
  pthread_mutex_lock(&state_mx);
  request = req;
  pthread_cond_broadcast(&state_cv);
  pthread_mutex_unlock(&state_mx);
  return 0;
}

int ltr_cal_shutdown()
{
  return signal_request(SHUTDOWN);
}

int ltr_cal_suspend()
{
  return signal_request(PAUSE);
}

int ltr_cal_wakeup()
{
  return signal_request(RUN);
}

enum cal_device_state_type ltr_cal_get_state()
{
  return tracker_state;
}
