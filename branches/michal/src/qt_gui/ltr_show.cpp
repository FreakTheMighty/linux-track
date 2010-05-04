#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QTimer>
#include <QThread>
#include <iostream>
#include <ltr_show.h>
#include <cal.h>
#include <utils.h>
#include <pref_global.h>
#include <pref_int.h>
#include <tracking.h>
#include <iostream>
#include <scp_form.h>

QImage *img;
QPixmap *pic;
QLabel *label;

unsigned char *bitmap = NULL;
bool flag;
static unsigned char *qt_bitmap = NULL;
static unsigned int w = 0;
static unsigned int h = 0;
static ScpForm *scp;

static enum state{TRACKER_RUNNING, TRACKER_PAUSED, TRACKER_STOPPED} state;

static bool should_start = false;

extern "C" {
  int frame_callback(struct camera_control_block *ccb, struct frame_type *frame);
}

static CaptureThread *ct = NULL;

CaptureThread::CaptureThread(LtrGuiForm *p): QThread(), parent(p)
{
}

void CaptureThread::setRunning()
{
  parent->setRunning();
}

void CaptureThread::run()
{
  static struct camera_control_block ccb;
  if(get_device(&ccb) == false){
    log_message("Can't get device category!\n");
    return;
  }
  init_tracking();
  ccb.mode = operational_3dot;
  ccb.diag = false;
  should_start = true;
  cal_run(&ccb, frame_callback);
  parent->setStopped();
}

void CaptureThread::signal_new_frame()
{
  emit new_frame();
}

LtrGuiForm::LtrGuiForm(ScpForm *s)
{
  scp = s;
  ui.setupUi(this);
  label = new QLabel();
  ui.pix_box->addWidget(label);
  ui.pauseButton->setDisabled(true);
  ui.wakeButton->setDisabled(true);
  ui.stopButton->setDisabled(true);
  glw = new Window();
  ui.ogl_box->addWidget(glw);
  ct = new CaptureThread(this);
  connect(ct, SIGNAL(new_frame()), this, SLOT(update()));
  setStopped();
}

LtrGuiForm::~LtrGuiForm()
{
  if(state != TRACKER_STOPPED){
    if(cal_shutdown() == 0){
      setStopped();
    }
    ct->wait(1000);
  }
  delete glw;
}


int frame_callback(struct camera_control_block *ccb, struct frame_type *frame)
{
  (void) ccb;
  static int cnt = 0;
  if((state != TRACKER_RUNNING) && (should_start)){
    should_start = false;
    ct->setRunning();
  }
  
  if(cnt == 0){
    recenter_tracking();
  }
  ++cnt;
  
  update_pose(frame);
  scp->updatePitch(lt_orig_pose.pitch);
  scp->updateRoll(lt_orig_pose.roll);
  scp->updateYaw(lt_orig_pose.heading);
  scp->updateX(lt_orig_pose.tx);
  scp->updateY(lt_orig_pose.ty);
  scp->updateZ(lt_orig_pose.tz);
  
  if((w != frame->width) || (h != frame->height)){
    w = frame->width;
    h = frame->height;
    if(bitmap != NULL){
      free(bitmap);
    }
    bitmap = (unsigned char *)my_malloc(frame->width * frame->height);
    if(qt_bitmap != NULL){
      free(qt_bitmap);
    }
    qt_bitmap = (unsigned char*)my_malloc(h * w * 3);
    img = new QImage(qt_bitmap, w, h, w * 3, QImage::Format_RGB888);
  }
  frame->bitmap = bitmap;
  if(flag == false){
    flag = true;
    ct->signal_new_frame();
  }
  return 0;
}


void LtrGuiForm::on_startButton_pressed()
{
  ct->start();
}

void LtrGuiForm::on_recenterButton_pressed()
{
  recenter_tracking();
}

void LtrGuiForm::on_pauseButton_pressed()
{
  if(cal_suspend() == 0){
    setPaused();
  }
}

void LtrGuiForm::on_wakeButton_pressed()
{
  should_start = true;
  cal_wakeup();
}


void LtrGuiForm::on_stopButton_pressed()
{
  if(cal_shutdown() == 0){
    ct->wait(1000);
  }
}

void LtrGuiForm::update()
{
  static int cnt = 0;
  cnt++;
  ui.statusbar->showMessage(QString("").setNum(cnt) + ". frame");
  
  unsigned char *src, *dst;
  unsigned int pixcnt;

  src = bitmap;
  dst = qt_bitmap;
  for(pixcnt = 0; pixcnt < w * h; ++pixcnt){
    *(dst++) = *src;
    *(dst++) = *src;
    *(dst++) = *src;
    *src = 0;
    src++;
  }
  
  label->setPixmap(pic->fromImage(*img));
  flag = false;
}

void LtrGuiForm::setStopped()
{
  state = TRACKER_STOPPED;
  ui.startButton->setDisabled(false);
  ui.pauseButton->setDisabled(true);
  ui.wakeButton->setDisabled(true);
  ui.stopButton->setDisabled(true);
  ui.recenterButton->setDisabled(true);
}

void LtrGuiForm::setRunning()
{
  state = TRACKER_RUNNING;
  ui.startButton->setDisabled(true);
  ui.pauseButton->setDisabled(false);
  ui.wakeButton->setDisabled(true);
  ui.stopButton->setDisabled(false);
  ui.recenterButton->setDisabled(false);
}

void LtrGuiForm::setPaused()
{
  state = TRACKER_PAUSED;
  ui.startButton->setDisabled(true);
  ui.pauseButton->setDisabled(true);
  ui.wakeButton->setDisabled(false);
  ui.stopButton->setDisabled(false);
  ui.recenterButton->setDisabled(true);
}
