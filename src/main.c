#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_output_layer;
static MenuLayer *menu_layer;
// static MenuLayer *menu_layer_h;
static Layer *window_layer;
ActionBarLayer *action_bar;
static GBitmap *dismiss, *check;

int samplingRate = ACCEL_SAMPLING_10HZ;
int sMinX = 1000;
int sMinY = 1000;
int sMinZ = 1000;
int h = 1000;
int fall = 0;
int menuFlag = 0;

enum CustomerDataKey {
    FALL = 0x0,
};

int calculateS(int accelerometerValue);
int decideFall(int s, int axis);
int getSminForAxis(int axis);
void setSminForAxisIfNeeded(int s,int axis);
static void fall_found();
void down_single_click_handler(ClickRecognizerRef recognizer, void *context);
void up_single_click_handler(ClickRecognizerRef recognizer, void *context);
void select_single_click_handler(ClickRecognizerRef recognizer, void *context);
void contact_android();
void startCUSUM();
void setSamplingRate();
void setThreshold();
void startMenu();
void config_provider(void *context);
void insertActionBarLayer();
//static void inbox_dropped_callback(AppMessageResult reason, void *context);
//static void inbox_received_callback(DictionaryIterator *iter, void *context);

static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
//   return MENU_CELL_BASIC_HEADER_HEIGHT;
    return 32;
}

static void data_handler(AccelData *data, uint32_t num_samples) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, " into data handler ");
  // Long lived buffer
  static char s_buffer[128];
  
  snprintf(s_buffer, sizeof(s_buffer), 
    "Hz %d , h %d \n N X,Y,Z\n0 %d,%d,%d\n1 %d,%d,%d\n2 %d,%d,%d,\n", 
    samplingRate,h,
    data[0].x, data[0].y, data[0].z, 
    data[1].x, data[1].y, data[1].z, 
    data[2].x, data[2].y, data[2].z
  );
  text_layer_set_text(s_output_layer, s_buffer);
  
  int i;
  
  for (i = 0; i<3; i++){
      int x = calculateS(data[i].x);
      int y = calculateS(data[i].y);
      int z = calculateS(data[i].z);

      int fallX = decideFall(x, 1); // axis == 1 for X 
      int fallY = decideFall(y, 2); // axis == 2 for Y
      int fallZ = decideFall(z, 3); // axis == 3 for Z 

      if( (fallX ==1 && fall ==0) || (fallY ==1 && fall ==0) || (fallZ ==1 && fall ==0)){
        fall = 1;
        accel_data_service_unsubscribe();
        APP_LOG(APP_LOG_LEVEL_DEBUG, " FALL ");
        fall_found();
      }
  }
}



static void fall_found(){  
  insertActionBarLayer();
  APP_LOG(APP_LOG_LEVEL_DEBUG, " FALL detected and into fall found function ");
  static char new_buffer[128];
  snprintf(new_buffer, sizeof(new_buffer),"you have fallen, do you need help ?");
  text_layer_set_text(s_output_layer, new_buffer);
  vibes_long_pulse();
}

void insertActionBarLayer(){
  check = gbitmap_create_with_resource(RESOURCE_ID_CHECK);
  dismiss = gbitmap_create_with_resource(RESOURCE_ID_DISMISS);
  
  action_bar = action_bar_layer_create();   // Initialize the action bar:
  action_bar_layer_add_to_window(action_bar, s_main_window);  // Associate the action bar with the window:
  action_bar_layer_set_click_config_provider(action_bar,config_provider);
  
  action_bar_layer_set_icon_animated(action_bar, BUTTON_ID_UP, check, true);
  action_bar_layer_set_icon_animated(action_bar, BUTTON_ID_DOWN, dismiss, true);
}

int calculateS(int accelerometerValue){
  int theta0 = 0;
  int theta1 = 1;
  int sigma = 2;
  
  //s will be multiplied by 1000 due to problems with floating points. divided into two calculations
  //s = (theta0 - theta1)/(sigma * sigma) * (accelerometerValue - (theta0 - theta1)/2);
  int first  = (theta0 - theta1) * 1000 / (sigma * sigma);
  int second = (accelerometerValue - (theta0 - theta1)/2);
  int s = first * second;
  return s;
}

int getSminForAxis(int axis){
  int sMin = 0;
  if( axis == 1){
    sMin = sMinX;
  }else if (axis == 2){ 
    sMin = sMinY;
  }else if ( axis == 3){
    sMin = sMinZ;
  }
  return sMin;
}

void setSminForAxisIfNeeded(int s,int axis){
  if( axis == 1){
    if (s < sMinX){
      sMinX = s;
    }
  }else if (axis == 2){
    if (s < sMinY){
      sMinY = s;
    }    
  }else if ( axis == 3){
    if (s < sMinZ){
      sMinZ = s;
    }
  }
}

int decideFall(int s, int axis){
  int sMin = getSminForAxis(axis);
  setSminForAxisIfNeeded(s,axis);
  if(s/1000 - sMin/1000 >= h){
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Fall detected in axis %d",axis);
    return 1; //sends 1 if fall detected
  }
  return 0;
}

void config_provider(void *context) {  //was Window *window
 // single click / repeat-on-hold config:
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
}

void resetMinAndFall(){
  sMinX = 1000;
  sMinY = 1000;
  sMinZ = 1000;
  fall =0;
}

void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  action_bar_layer_destroy(action_bar); 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "## action bar destroyed ##");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "@@@@@ RESTART ACCELLEROMETER @@@@@");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "@@@@@ smin and fall set to starting values @@@@@");
  if(fall == 1){
    resetMinAndFall();
    int num_samples = 3;
    accel_data_service_subscribe(num_samples, data_handler);

    // Choose update rate
    accel_service_set_sampling_rate(samplingRate);
  }
}


void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
action_bar_layer_destroy(action_bar);
APP_LOG(APP_LOG_LEVEL_DEBUG, "*********** CONTACT ANDROID APP *************");
    contact_android();
}

void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
action_bar_layer_destroy(action_bar);
APP_LOG(APP_LOG_LEVEL_DEBUG, "*********** CONTACT ANDROID APP *************");
    contact_android();
}

void contact_android(){
  if(fall == 1){
    DictionaryIterator *out_iter;
    // Prepare the outbox buffer for this message
    AppMessageResult result = app_message_outbox_begin(&out_iter);
    if(result == APP_MSG_OK) {
      Tuplet value = TupletCString(FALL,"FALL");
      dict_write_tuplet(out_iter, &value);

      // Send this message
      result = app_message_outbox_send();
      static char new_buffer[128];
      snprintf(new_buffer, sizeof(new_buffer),"Message Sent");
      text_layer_set_text(s_output_layer, new_buffer);
      if(result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
      }
    } else {
      // The outbox cannot be used right now
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
    }
  }else{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "########### FALL NOT YET DETECTED ##############");
  }
}

void outbox_sent_callback(DictionaryIterator *iter, void *context) {
  // The message just sent has been successfully delivered
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message SENT");
  static char new_buffer[128];
  snprintf(new_buffer, sizeof(new_buffer),"Phone RECEIVED message");
  text_layer_set_text(s_output_layer, new_buffer);
}

void outbox_failed_callback(DictionaryIterator *iter,
                                      AppMessageResult reason, void *context) {
  // The message just sent failed to be delivered
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message send FAILED. Reason: %d", (int)reason);
  static char new_buffer[128];
  snprintf(new_buffer, sizeof(new_buffer),"Message FAILED to sent");
  text_layer_set_text(s_output_layer, new_buffer);
}

static void setupAppMessage(void){
    //app message callbacks registered
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  const int inbound_size = 64;
  const int outbound_size = 64;
  
  app_message_open(inbound_size, outbound_size);
}


/* CODE FOR MENU */
uint16_t num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
  if(menuFlag == 0){
    return 2;
  }else if(menuFlag == 1){
    return 4;
  }if(menuFlag == 2){
    return 7;
  }else{
    return 2;
  }
}

void draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context){
  if (menuFlag == 0){
    switch (section_index) {
      case 0:
        // Draw title text in the section header
        menu_cell_title_draw (ctx, cell_layer, "App options");
        break;
    }
  }else if(menuFlag == 1){
    switch (section_index) {
      case 0:
        // Draw title text in the section header
        menu_cell_title_draw(ctx, cell_layer, "Sampling Rate");
        break;
    } 
  }else if(menuFlag == 2){
    switch (section_index) {
      case 0:
        // Draw title text in the section header
        menu_cell_title_draw(ctx, cell_layer, "Threshold");
        break;
    } 
  }
}

void draw_row_callback(GContext *ctx, Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
  if(menuFlag == 0){  //predifined or custom value
      switch (cell_index->section) {
        case 0:
        switch(cell_index->row){
        case 0:
            menu_cell_basic_draw(ctx, cell_layer, "1.Start APP", "Predifined parameters", NULL);
            break;
        case 1:
            menu_cell_basic_draw(ctx, cell_layer, "2.Custom CUSUM", "Define values", NULL);
            break;
        } 
        break ;
      }
  }
  if(menuFlag == 1){  //sampling rate menu
    switch (cell_index->section) {
      case 0:
      switch(cell_index->row){
        case 0:
            menu_cell_basic_draw(ctx, cell_layer, "10 Hz", NULL, NULL);
            break;
        case 1:
            menu_cell_basic_draw(ctx, cell_layer, "25 Hz", NULL, NULL);
            break;
        case 2:
            menu_cell_basic_draw(ctx, cell_layer, "50 Hz", NULL, NULL);
            break;
        case 3:
            menu_cell_basic_draw(ctx, cell_layer, "100 Hz", NULL, NULL);
            break;
      }
      break ;
    }
  }
  if(menuFlag == 2){  //thresshold menu
    switch (cell_index->section) {
      case 0:
      switch(cell_index->row){
        case 0:
            menu_cell_basic_draw(ctx, cell_layer, "100", NULL, NULL);
            break;
        case 1:
            menu_cell_basic_draw(ctx, cell_layer, "250", NULL, NULL);
            break;
        case 2:
            menu_cell_basic_draw(ctx, cell_layer, "500", NULL, NULL);
            break;
        case 3:
            menu_cell_basic_draw(ctx, cell_layer, "650", NULL, NULL);
            break;
        case 4:
            menu_cell_basic_draw(ctx, cell_layer, "750", NULL, NULL);
            break;
        case 5:
            menu_cell_basic_draw(ctx, cell_layer, "875", NULL, NULL);
            break;
        case 6:
            menu_cell_basic_draw(ctx, cell_layer, "1000", NULL, NULL);
            break;   
      }
      break ;
    }
  }
}


void select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context)
{  
    int which = cell_index->row;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Buuton pressedn in menu %d",which);
  
    if(menuFlag == 0){
     if(which == 0){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Start CUSUM");
        startCUSUM();
      }else if (which == 1){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Give alter values menu");
        setSamplingRate();
      }
    }else if(menuFlag == 1){
      if(which == 0){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hz = 10 %d ", ACCEL_SAMPLING_10HZ );
        samplingRate = ACCEL_SAMPLING_10HZ;
      }else if (which == 1){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hz = 25");
        samplingRate = ACCEL_SAMPLING_25HZ ;
      }else if (which == 2){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hz = 50");
        samplingRate = ACCEL_SAMPLING_50HZ ;
      }else if (which == 3){
        APP_LOG(APP_LOG_LEVEL_DEBUG, "hz = 100");
        samplingRate = ACCEL_SAMPLING_100HZ ;
      } 
      setThreshold();
    } else if(menuFlag == 2){
      if(which == 0){
        h = 100;
      }else if (which == 1){
        h = 250;
      }else if (which == 2){
        h = 500;
      }else if (which == 3){
        h = 650;
      }else if (which == 4){
        h = 750;
      }else if (which == 5){
        h = 850;
      }else if (which == 6){
        h = 1000;
      } 
       APP_LOG(APP_LOG_LEVEL_DEBUG, "h %d",h);
       startCUSUM();
    }
    //Get which row

    //The array that will hold the on/off vibration times

}

/* END of CODE FOR MENU */

void startCUSUM(){
   APP_LOG(APP_LOG_LEVEL_DEBUG, " accelerometer started? cusum started? ");
  
  text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_output_layer, "No data yet.");
  text_layer_set_overflow_mode(s_output_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_output_layer));
  
//    menu_layer_destroy(menu_layer);
  
  int num_samples = 3;
  accel_data_service_subscribe(num_samples, data_handler);

  // Choose update rate
  accel_service_set_sampling_rate(samplingRate);

//   window_set_click_config_provider(s_main_window, (ClickConfigProvider) config_provider);

  setupAppMessage();
}

void setSamplingRate(){
  menuFlag = 1;
  startMenu();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Variables will be set");
}

void setThreshold(){
  menuFlag = 2;
  startMenu();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Variables will be set");
}

void startMenu(){
    MenuLayerCallbacks callbacks = {
      .get_header_height = menu_get_header_height_callback,
      .draw_header = (MenuLayerDrawHeaderCallback ) draw_header_callback,
      .draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
      .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) num_rows_callback,
      .select_click = (MenuLayerSelectCallback) select_click_callback
  };
  menu_layer_set_callbacks(menu_layer, NULL, callbacks);
}

static void main_window_load(Window *window) {
  window_layer = window_get_root_layer(window);
//   GRect window_bounds = layer_get_bounds(window_layer);

//   s_output_layer = text_layer_create(GRect(5, 0, window_bounds.size.w - 10, window_bounds.size.h));
  s_output_layer = text_layer_create(GRect(0, 0, 144, 168 - 16));
//   text_layer_set_font(s_output_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
//   text_layer_set_text(s_output_layer, "No data yet.");
//   text_layer_set_overflow_mode(s_output_layer, GTextOverflowModeWordWrap);
//   layer_add_child(window_layer, text_layer_get_layer(s_output_layer));
  
      //Create it - 12 is approx height of the top bar
    menu_layer = menu_layer_create(GRect(0, 0, 144, 168 - 16));
 
    //Let it receive clicks
    menu_layer_set_click_config_onto_window(menu_layer, window);
   
    startMenu();
  
    check = gbitmap_create_with_resource(RESOURCE_ID_CHECK);
  dismiss = gbitmap_create_with_resource(RESOURCE_ID_DISMISS);
  
  action_bar = action_bar_layer_create();   // Initialize the action bar:
  
  
  action_bar_layer_set_icon_animated(action_bar, BUTTON_ID_UP, check, true);
  action_bar_layer_set_icon_animated(action_bar, BUTTON_ID_DOWN, dismiss, true);
  
//     MenuLayerCallbacks callbacks = {
//         .draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
//         .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) num_rows_callback,
//         .select_click = (MenuLayerSelectCallback) select_click_callback
//     };
//     menu_layer_set_callbacks(menu_layer, NULL, callbacks);
 
    //Add to Window
    layer_add_child(window_get_root_layer(window), menu_layer_get_layer(menu_layer));

}

static void main_window_unload(Window *window) {
  // Destroy output TextLayer
  text_layer_destroy(s_output_layer);
  

  menu_layer_destroy(menu_layer);

}


static void init() {
  // Create main Window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  // Subscribe to the accelerometer data service
  
//   int num_samples = 3;
//   accel_data_service_subscribe(num_samples, data_handler);

//   // Choose update rate
//   accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);

//   window_set_click_config_provider(s_main_window, (ClickConfigProvider) config_provider);

//   setupAppMessage();
}

static void deinit() {
  // Destroy main Window
  window_destroy(s_main_window);

//   accel_data_service_unsubscribe();
//   app_message_deregister_callbacks();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}