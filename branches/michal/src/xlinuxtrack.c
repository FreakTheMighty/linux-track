#include "XPLMGraphics.h"
#include "XPLMMenus.h"
#include "XPLMDisplay.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPWidgets.h"
#include "XPStandardWidgets.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <math_utils.h>
#include "ltlib.h"

XPLMHotKeyID		gFreezeKey = NULL;
XPLMHotKeyID		gTrackKey = NULL;

XPLMDataRef		head_x = NULL;
XPLMDataRef		head_y = NULL;
XPLMDataRef		head_z = NULL;
XPLMDataRef		head_psi = NULL;
XPLMDataRef		head_the = NULL;

XPWidgetID		setupWindow = NULL;
XPWidgetID		mapText = NULL;
XPWidgetID		saveButton = NULL;
			
XPLMMenuID		setupMenu = NULL;
			
XPWidgetID		jmWindow;
XPWidgetID		jmText;
XPWidgetID		jmButton;
int			jmWindowOpened = 0;
int			jmRun = 0;

XPLMDataRef		joy_buttons = NULL;
int 			buttons[1520];

int 			buttonIndex = -1;
char			text[150];

int 			freeze_button = -1;
int			recenter_button = -1;

float			debounce_time = 0.01;

int button_array_size = 0;
int pos_init_flag = 0;
bool freeze = false;

float base_x;
float base_y;
float base_z;
bool active_flag = false;






void	MyHotKeyCallback(void *               inRefcon);    

int	AircraftDrawCallback(	XPLMDrawingPhase     inPhase,
                          int                  inIsBefore,
                          void *               inRefcon);

float	joystickCallback(
                                   float                inElapsedSinceLastCall,    
                                   float                inElapsedTimeSinceLastFlightLoop,    
                                   int                  inCounter,    
                                   void *               inRefcon);

bool createSetupWindow(int x, int y, int w, int h);

struct buttonDef{
  char *caption;
  XPWidgetID text;
  XPWidgetID button;
  char *prefName;
  pref_id pref;
};

struct buttonDef btArray[] = {
  {
    .caption = "Start/Stop tracking:",
    .prefName = "Recenter-button"
  },
  {
    .caption = "Tracking freeze:",
    .prefName = "Freeze-button"
  }
};

struct scrollerDef{
  char *caption;
  float min;
  float max;
  XPWidgetID text;
  XPWidgetID scroller;
  int dx;
  int dy;
  int w;
  char *prefName;
  pref_id pref;
};

struct scrollerDef scArray[] = {
  {.caption = "Pitch sensitivity", 
   .min = 0.0,
   .max = 10.0,
   .dx = 20, 
   .dy = 50,
   .w = 160,
   .prefName = "Pitch-multiplier"},
  {.caption = "Yaw sensitivity", 
   .min = 0.0,
   .max = 10.0,
   .dx = 0, 
   .dy = 30,
   .w = 160,
   .prefName = "Yaw-multiplier"},
  {.caption = "X-translation sensitivity", 
   .min = 0.0,
   .max = 10.0,
   .dx = 0, 
   .dy = 30,
   .w = 160,
   .prefName = "Xtranslation-multiplier"},
  {.caption = "Y-translation sensitivity", 
   .min = 0.0,
   .max = 10.0,
   .dx = 0, 
   .dy = 30,
   .w = 160,
   .prefName = "Ytranslation-multiplier"},
  {.caption = "Z-translation sensitivity", 
   .min = 0.0,
   .max = 10.0,
   .dx = 0, 
   .dy = 30,
   .w = 160,
   .prefName = "Ztranslation-multiplier"},
  {.caption = "Filter factor", 
   .min = 0.0,
   .max = 50.0,
   .dx = 0, 
   .dy = 30,
   .w = 160,
   .prefName = "Filter-factor"}
};





void linuxTrackMenuHandler(void *inMenuRef, void *inItemRef)
{
  (void) inMenuRef;
  (void) inItemRef;
  if(setupWindow == NULL){
    createSetupWindow(100, 600, 300, 370);
  }else{
    if(!XPIsWidgetVisible(setupWindow)){
      XPShowWidget(setupWindow);
    }
  }
  return;
}

int jmProcessJoy()
{
  int new_buttons[1520];
  int i = 0;
  XPLMGetDatavi(joy_buttons, new_buttons, 0, button_array_size);
  for(i = 0;i < button_array_size;++i){
    if(new_buttons[i] != buttons[i]){
      return i;
    }
  }
  return -1.0;
}

bool updateButtonCaption(int index, int button)
{
  if(index < 0){
    return false;
  }
  if(button >= 0){
    sprintf(text, "%s button %d", btArray[index].caption, button);
  }else{
    sprintf(text, "%s Not mapped", btArray[index].caption);
  }
  XPSetWidgetDescriptor(btArray[index].text, text);
  
  if(button < 0){
    return true;
  }
  
  lt_set_int(&(btArray[index].pref), button);
  switch(index){
    case 0:
      recenter_button = button;
      break;
    case 1:
      freeze_button = button;
      break;
    default:
      break;
  };
  return true;
}

int jmWindowHandler(XPWidgetMessage inMessage,
			XPWidgetID inWidget,
			long inParam1,
			long inParam2)
{
  (void) inWidget;
  (void) inParam1;
  (void) inParam2;

  if(inMessage == xpMsg_PushButtonPressed){
    //there is only one button
    jmRun = 0;
    if(jmWindow != NULL){
      jmWindowOpened = 0;
      buttonIndex = -1;
      XPHideWidget(jmWindow);
    }
    return 1;
  }
  return 0;
}

int joyMapDialog(char *caption)
{
  if(jmWindowOpened != 0){
    return -1;
  }
  jmWindowOpened = 1;
  
  if(jmWindow != NULL){
    XPSetWidgetDescriptor(jmText, caption);
    XPShowWidget(jmWindow);
  }else{
    int x  = 100;
    int y  = 600;
    int w  = 300;
    int h  = 100;
    int x2 = x + w;
    int y2 = y - h;

    jmWindow = XPCreateWidget(x, y, x2, y2,
  				  1, //Visible
				  "Joystick button mapping...",
				  1, //Root
				  NULL, //No container
				  xpWidgetClass_MainWindow 
    );
    jmText = XPCreateWidget(x+20, y - 20, x2 -20, y -40 ,
    				   1, caption, 0, jmWindow, 
				   xpWidgetClass_Caption);
    jmButton = XPCreateWidget(x+80, y2+40, x2-80, y2+20, 1, 
  				  "Cancel", 0, jmWindow,
  				  xpWidgetClass_Button);
    XPLMGetDatavi(joy_buttons, buttons, 0, button_array_size);
    XPAddWidgetCallback(jmWindow, jmWindowHandler);
  }
  jmRun = 1;
  return 0;
}

bool updateScrollerCaption(int i)
{
  struct scrollerDef *sc = &(scArray[i]);
  long pos = XPGetWidgetProperty(sc->scroller, 
               xpProperty_ScrollBarSliderPosition, NULL);
  float val = sc->min + (sc->max - sc->min) * pos / 100.0;
  sprintf(text, "%s = %g", sc->caption, val);
  XPSetWidgetDescriptor(sc->text, text);
  lt_set_flt(&(scArray[i].pref), val);
  return true;
}



int setupWindowHandler(XPWidgetMessage inMessage,
			XPWidgetID inWidget,
			long inParam1,
			long inParam2)
{
  (void) inWidget;
  (void) inParam2;
  if(inMessage == xpMessage_CloseButtonPushed){
    if(setupWindow != NULL){
      XPHideWidget(setupWindow);
    }
    return 1;
  }
  if(inMessage == xpMsg_PushButtonPressed){
    if(inParam1 == (long)btArray[0].button){
      buttonIndex = 0;
      joyMapDialog("Remap joystick button for Start/Stop Tracking");
    }
    if(inParam1 == (long)btArray[1].button){
      buttonIndex = 1;
      joyMapDialog("Remap joystick button for Tracking freeze");
    }
    if(inParam1 == (long)saveButton){
      lt_save_prefs();
    }
  }
  if(inMessage == xpMsg_ScrollBarSliderPositionChanged){
    int i;
    for(i = 0; i < 6; ++i){
      if(inParam1 == (long)scArray[i].scroller){
        updateScrollerCaption(i);
	break;
      }
    }
  }
  
  return 0;
}

bool setupScrollers(XPWidgetID *window, int x, int y)
{
  int i;
  int px = x;
  int py = y;
  int w;
  XPWidgetID sc;
  
  for(i = 0; i < 6; ++i){
    px += scArray[i].dx;
    py -= scArray[i].dy;
    w = scArray[i].w;
    
    sc = scArray[i].scroller = XPCreateWidget(px, py, px + w, py - 10 , 1, 
  				  scArray[i].caption, 0, *window,
  				  xpWidgetClass_ScrollBar 
    );
    XPSetWidgetProperty(sc, xpProperty_ScrollBarType, xpScrollBarTypeSlider);
    XPSetWidgetProperty(sc, xpProperty_ScrollBarMin, 0);
    XPSetWidgetProperty(sc, xpProperty_ScrollBarMax, 100);
    XPSetWidgetProperty(sc, xpProperty_ScrollBarPageAmount, 10);
    
    if(!lt_open_pref(scArray[i].prefName, &(scArray[i].pref))){
      scArray[i].pref = NULL;
    }
    
    
    float prf = lt_get_flt(scArray[i].pref);
    long pos = 100 * (prf - scArray[i].min) / (scArray[i].max - scArray[i].min);
    if(pos > 100){
      pos = 100;
    }else if(pos < 0){
      pos = 0;
    }
    XPSetWidgetProperty(sc, xpProperty_ScrollBarSliderPosition, pos);
    
    scArray[i].text = XPCreateWidget(px, py + 18, px + w, py + 5 ,
    				 1, scArray[i].caption, 0, *window,
  				  xpWidgetClass_Caption);
    updateScrollerCaption(i);
  }
  return true;
}

bool get_pref(char *name, pref_id *prf)
{
  if(!lt_open_pref(name, prf)){
    if(lt_create_pref(name)){
      if(!lt_open_pref(name, prf)){
        prf = NULL;
        lt_log_message("Couldn't open newly created pref '%s'!\n", 
	            name);
      }
    }else{
      *prf = NULL;
      lt_log_message("Couldn't create pref '%s'!\n", name);
    }
  }
  return true;
}

bool createSetupWindow(int x, int y, int w, int h)
{
  int x2 = x + w;
  int y2 = y - h;
  
  setupWindow = XPCreateWidget(x, y, x2, y2,
  				1, //Visible
				"Linux Track Setup",
				1, //Root
				NULL, //No container
				xpWidgetClass_MainWindow 
  );
  
  XPSetWidgetProperty(setupWindow, xpProperty_MainWindowHasCloseBoxes, 1);
  
  XPWidgetID sw = XPCreateWidget(x+10, y-30, x2-10, y2+10, 
  				1, //Visible
				"", //Desc
				0, //Root
				setupWindow,
				xpWidgetClass_SubWindow
  );
  
  XPSetWidgetProperty(sw, xpProperty_SubWindowType, 
  				xpSubWindowStyle_SubWindow
  );
  
  mapText = XPCreateWidget(x+20, y2 + 140, x2 -20, y2 + 120 ,
    				 1, "Joystick button mapping",
				 0, setupWindow, xpWidgetClass_Caption);
  btArray[0].button = XPCreateWidget(x2-120, y2+120, x2-20, y2+100, 1, 
  				"Remap", 0, setupWindow,
  				xpWidgetClass_Button);
  btArray[0].text = XPCreateWidget(x+20, y2 + 120, x2 -140, y2 + 100 ,
    				 1, btArray[0].caption,
				 0, setupWindow, xpWidgetClass_Caption);
  get_pref(btArray[0].prefName, &(btArray[0].pref));
  btArray[1].button = XPCreateWidget(x2-120, y2+90, x2-20, y2+70, 1, 
  				"Remap", 0, setupWindow,
  				xpWidgetClass_Button);
  btArray[1].text = XPCreateWidget(x+20, y2 + 90, x2 -140, y2 + 70 ,
    				 1, btArray[1].caption,
				 0, setupWindow, xpWidgetClass_Caption);
  get_pref(btArray[1].prefName, &(btArray[1].pref));
  saveButton = XPCreateWidget(x+20, y2+30, x2-20, y2+10, 1, 
  				"Save preferences", 0, setupWindow,
  				xpWidgetClass_Button);
  XPSetWidgetProperty(btArray[0].button, xpProperty_ButtonType, xpPushButton);
  XPSetWidgetProperty(btArray[1].button, xpProperty_ButtonType, xpPushButton);
  XPSetWidgetProperty(saveButton, xpProperty_ButtonType, xpPushButton);
  updateButtonCaption(0, recenter_button);
  updateButtonCaption(1, freeze_button);

  setupScrollers(&setupWindow, x, y);

  XPAddWidgetCallback(setupWindow, setupWindowHandler);
  return true;
}


PLUGIN_API int XPluginStart(
                            char *		outName,
                            char *		outSig,
                            char *		outDesc)
{
  strcpy(outName, "linuxTrack");
  strcpy(outSig, "linuxtrack.camera");
  strcpy(outDesc, "A plugin that controls view using your webcam.");

  /* Register our hot key for the new view. */
  gTrackKey = XPLMRegisterHotKey(XPLM_VK_F8, xplm_DownFlag, 
                         "3D linuxTrack view",
                         MyHotKeyCallback,
                         (void*)0);
  gFreezeKey = XPLMRegisterHotKey(XPLM_VK_F9, xplm_DownFlag, 
                         "Freeze 3D linuxTrack view",
                         MyHotKeyCallback,
                         (void*)1);
  head_x = XPLMFindDataRef("sim/graphics/view/pilots_head_x");
  head_y = XPLMFindDataRef("sim/graphics/view/pilots_head_y");
  head_z = XPLMFindDataRef("sim/graphics/view/pilots_head_z");
  
  head_psi = XPLMFindDataRef("sim/graphics/view/pilots_head_psi");
  head_the = XPLMFindDataRef("sim/graphics/view/pilots_head_the");
  joy_buttons = XPLMFindDataRef("sim/joystick/joystick_button_values");
  
  int xplane_ver;
  int sdk_ver;
  XPLMHostApplicationID app_id;
  XPLMGetVersions(&xplane_ver, &sdk_ver, &app_id);
  if(xplane_ver < 850){
    button_array_size = 64;
  }else if(xplane_ver < 900){
    button_array_size = 160;
  }else{
    button_array_size = 1520;
  }
  printf("%d joystick buttons\n", button_array_size);
  
  if((head_x==NULL)||(head_y==NULL)||(head_z==NULL)||
     (head_psi==NULL)||(head_the==NULL)||(joy_buttons==NULL)){
    return(0);
  }
  if(lt_init("XPlane")!=0){
    return(0);
  }
  lt_suspend();
  XPLMRegisterFlightLoopCallback(		
	joystickCallback,	/* Callback */
	-1.0,					/* Interval */
	NULL);					/* refcon not used. */
  pref_id frb, rcb;
  if(lt_open_pref("Freeze-button", &frb)){
    freeze_button = lt_get_int(frb);
    lt_close_pref(&frb);
  }else{
    freeze_button = -1;
    lt_log_message("Couldn't find Freeze-buton definition!\n");
  }
  if(lt_open_pref("Recenter-button", &rcb)){
    recenter_button = lt_get_int(rcb);
    lt_close_pref(&rcb);
  }else{
    recenter_button = -1;
    lt_log_message("Couldn't find Recenter-buton definition!\n");
  }
  int index = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "LinuxTrack", NULL, 1);


  setupMenu = XPLMCreateMenu("LinuxTrack", XPLMFindPluginsMenu(), index, 
                         linuxTrackMenuHandler, NULL);
  XPLMAppendMenuItem(setupMenu, "Setup", (void *)"Setup", 1);
  
  return(1);
}

PLUGIN_API void	XPluginStop(void)
{
  XPLMUnregisterHotKey(gTrackKey);
  XPLMUnregisterHotKey(gFreezeKey);
  XPLMUnregisterFlightLoopCallback(joystickCallback, NULL);
  lt_shutdown();
}

PLUGIN_API void XPluginDisable(void)
{
}

PLUGIN_API int XPluginEnable(void)
{
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(
                                      XPLMPluginID	inFromWho,
                                      long			inMessage,
                                      void *			inParam)
{
  (void) inFromWho;
  (void) inMessage;
  (void) inParam;
}

void activate(void)
{
	  active_flag=true;
          pos_init_flag = 1;
	  freeze = false;
          lt_recenter();
	  XPLMCommandKeyStroke(xplm_key_forward);
          XPLMRegisterDrawCallback(
                             AircraftDrawCallback,
                             xplm_Phase_LastCockpit,
                             0,
                             NULL);
	  lt_wakeup();
}

void deactivate(void)
{
	  active_flag=false;
	  XPLMUnregisterDrawCallback(
                               AircraftDrawCallback,
                               xplm_Phase_LastCockpit,
                               0,
                               NULL);
                               
          XPLMSetDataf(head_x,base_x);
          XPLMSetDataf(head_y,base_y);
          XPLMSetDataf(head_z,base_z);
	  XPLMSetDataf(head_psi,0.0f);
	  XPLMSetDataf(head_the,0.0f);
	  lt_suspend();
}

void	MyHotKeyCallback(void *               inRefcon)
{
  switch((int)inRefcon){
    case 0:
      if(active_flag==false){
	activate();
      }else{
	deactivate();
      }
      break;
    case 1:
      freeze = (freeze == false)? true : false;
      break;
  }
}

void joy_fsm(int button, int *state, float *ts, bool *flag)
{
  switch(*state){
    case 1: //Waiting for button to be pressed
      if(button != 0){
        *ts = XPLMGetElapsedTime();
	*state = 2;
      }
      break;
    case 2: //Counting...
      if(button == 0){
        *state = 1; //button was released, go back
      }else{
        if((XPLMGetElapsedTime() - *ts) > debounce_time){
	  *flag = (*flag == false)? true : false;
	  *state = 3;
	}
      }
      break;
    case 3: //Waiting for button to be released
      if(button == 0){
        *ts = XPLMGetElapsedTime();
	*state = 4;
      }
      break;
    case 4:
      if(button != 0){
        *state = 3; //button was pressed again, go back
      }else{
        if((XPLMGetElapsedTime() - *ts) > debounce_time){
	  *state = 1;
	}
      }
      break;
    default:
      lt_log_message("Joystick button processing got into wrong state (%d)!\n", *state);
      *state = 1;
      break;
  }
}

void process_joy()
{
  static float freeze_ts;
  static float recenter_ts;
  static int freeze_state = 1;
  static int recenter_state = 1;
  
  XPLMGetDatavi(joy_buttons, buttons, 0, 1520);
  
  if(freeze_button != -1){
    joy_fsm(buttons[freeze_button], &freeze_state, &freeze_ts, &freeze);
  }
  if(recenter_button != -1){
    joy_fsm(buttons[recenter_button], &recenter_state, &recenter_ts, &active_flag);
  }
}

float	joystickCallback(
                                   float                inElapsedSinceLastCall,    
                                   float                inElapsedTimeSinceLastFlightLoop,    
                                   int                  inCounter,    
                                   void *               inRefcon)
{
  (void) inElapsedSinceLastCall;
  (void) inElapsedTimeSinceLastFlightLoop;
  (void) inCounter;
  (void) inRefcon;
	if(jmRun != 0){
	  int res = jmProcessJoy();
	  if(res != -1){
	    updateButtonCaption(buttonIndex, res);
            XPHideWidget(jmWindow);
            jmWindowOpened = 0;
	    jmRun = 0;
	  }
	}else{
          bool last_active_flag = active_flag;
          process_joy();
          if(last_active_flag != active_flag){
	    if(active_flag){
	      activate();
	    }else{
	      deactivate();
	    }
	  }
	}
	return -1.0;
}                                   



int	AircraftDrawCallback(	XPLMDrawingPhase     inPhase,
                          int                  inIsBefore,
                          void *               inRefcon)
{
  (void) inPhase;
  (void) inIsBefore;
  (void) inRefcon;
  float heading, pitch, roll;
  float tx, ty, tz;
  int retval;
  
  retval = lt_get_camera_update(&heading,&pitch,&roll,
                                &tx, &ty, &tz);

  if (retval < 0) {
    printf("xlinuxtrack: Error code %d detected!\n", retval);
    return 1;
  }
  if(is_finite(heading) && is_finite(pitch) && is_finite(roll) &&
     is_finite(tx) && is_finite(ty) && is_finite(tz)){
    // Empty
  }else{
    lt_log_message("Bad values!\n");
    return 1;
  }

  if(freeze == true){
    return 1;
  }
  
  tx *= 1e-3;
  ty *= 1e-3;
  tz *= 1e-3;
/*   printf("heading: %f\tpitch: %f\n", heading, pitch); */ 
/*   printf("tx: %f\ty: %f\tz: %f\n", tx, ty, tz); */

  /* Fill out the camera position info. */
  /* FIXME: not doing translation */
  /* FIXME: not roll, is this even possible? */

  if(pos_init_flag == 1){
    pos_init_flag = 0;
    base_x = XPLMGetDataf(head_x);
    base_y = XPLMGetDataf(head_y);
    base_z = XPLMGetDataf(head_z);
  }
    
  XPLMSetDataf(head_x,base_x + tx);
  XPLMSetDataf(head_y,base_y + ty);
  XPLMSetDataf(head_z,base_z + tz);
  XPLMSetDataf(head_psi,heading);
  XPLMSetDataf(head_the,pitch);
	return 1;
}                                   

//positive x moves us to the right (meters?)
//positive y moves us up
//positive z moves us back