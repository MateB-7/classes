#include <pebble.h>

static Window *window;
static TextLayer *time_layer;
static TextLayer *schedule_layer;

// Class schedule structure
typedef struct {
  int start_hour;
  int start_min;
  int end_hour;
  int end_min;
  char name[32];
  char label[8];
} ClassPeriod;

// Default schedules - will be overwritten by saved settings
static ClassPeriod schedule[5][2] = {
  // Monday
  {{11, 0, 12, 30, "Tech", "P1"},
   {13, 0, 14, 50, "Campion", "P2"}},
  // Tuesday
  {{11, 0, 12, 30, "STEM", "P1"},
   {13, 0, 14, 50, "Campion", "P2"}},
  // Wednesday
  {{11, 0, 12, 30, "Math", "P1"},
   {13, 0, 14, 50, "Tech", "P2"}},
  // Thursday
  {{11, 0, 12, 30, "Math", "P1"},
   {13, 0, 14, 50, "Sport", "P2"}},
  // Friday
  {{11, 0, 12, 30, "Tech", "P1"},
   {13, 0, 14, 50, "Campion", "P2"}}
};

static int periods_per_day = 2;

// Load schedule from persistent storage
static void load_schedule() {
  for (int day = 0; day < 5; day++) {
    for (int period = 0; period < 2; period++) {
      int key = day * 2 + period;
      
      if (persist_exists(key)) {
        char saved_name[32];
        persist_read_string(key, saved_name, sizeof(saved_name));
        strncpy(schedule[day][period].name, saved_name, sizeof(schedule[day][period].name));
      }
    }
  }
}

static ClassPeriod* get_schedule(int day_of_week) {
  if (day_of_week < 1 || day_of_week > 5) return NULL;
  return schedule[day_of_week - 1];
}

static void update_schedule() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  int current_mins = tick_time->tm_hour * 60 + tick_time->tm_min;
  int day_of_week = tick_time->tm_wday;
  
  static char time_buffer[32];
  static char schedule_buffer[256];
  
  // Format current time and day
  strftime(time_buffer, sizeof(time_buffer), "%a %I:%M %p", tick_time);
  text_layer_set_text(time_layer, time_buffer);
  
  // Get schedule for current day
  ClassPeriod *today = get_schedule(day_of_week);
  
  if (today == NULL) {
    snprintf(schedule_buffer, sizeof(schedule_buffer), "\n\nNo classes\ntoday!\n\nEnjoy your\nweekend!");
    text_layer_set_text(schedule_layer, schedule_buffer);
    return;
  }
  
  // Find current class
  int current_class = -1;
  for (int i = 0; i < periods_per_day; i++) {
    int start_mins = today[i].start_hour * 60 + today[i].start_min;
    int end_mins = today[i].end_hour * 60 + today[i].end_min;
    
    if (current_mins >= start_mins && current_mins < end_mins) {
      current_class = i;
      break;
    }
  }
  
  // Build schedule display with times
  schedule_buffer[0] = '\0';
  
  for (int i = 0; i < periods_per_day; i++) {
    char line[80];
    char start_time[16];
    char end_time[16];
    
    // Format times in 12-hour format
    int start_hour = today[i].start_hour;
    int end_hour = today[i].end_hour;
    
    snprintf(start_time, sizeof(start_time), "%d:%02d%s", 
             start_hour > 12 ? start_hour - 12 : (start_hour == 0 ? 12 : start_hour),
             today[i].start_min,
             start_hour >= 12 ? "pm" : "am");
    
    snprintf(end_time, sizeof(end_time), "%d:%02d%s",
             end_hour > 12 ? end_hour - 12 : (end_hour == 0 ? 12 : end_hour),
             today[i].end_min,
             end_hour >= 12 ? "pm" : "am");
    
    if (i == current_class) {
      // Highlight current class
      snprintf(line, sizeof(line), "** NOW **\n%s: %s\n%s-%s\n*********\n", 
               today[i].label, today[i].name, start_time, end_time);
    } else {
      snprintf(line, sizeof(line), "%s: %s\n%s-%s\n", 
               today[i].label, today[i].name, start_time, end_time);
    }
    strcat(schedule_buffer, line);
  }
  
  // Add status message at bottom
  if (current_class < 0) {
    if (current_mins < today[0].start_hour * 60 + today[0].start_min) {
      strcat(schedule_buffer, "\n(Before classes)");
    } else {
      strcat(schedule_buffer, "\n(Classes done!)");
    }
  }
  
  text_layer_set_text(schedule_layer, schedule_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_schedule();
}

// Handle incoming messages from phone config
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Parse the received data
  Tuple *day_tuple = dict_find(iter, MESSAGE_KEY_Day);
  Tuple *period_tuple = dict_find(iter, MESSAGE_KEY_Period);
  Tuple *name_tuple = dict_find(iter, MESSAGE_KEY_ClassName);
  
  if (day_tuple && period_tuple && name_tuple) {
    int day = day_tuple->value->int32;
    int period = period_tuple->value->int32;
    char *class_name = name_tuple->value->cstring;
    
    // Calculate storage key
    int key = day * 2 + period;
    
    // Save to persistent storage
    persist_write_string(key, class_name);
    
    // Update in-memory schedule
    if (day >= 0 && day < 5 && period >= 0 && period < 2) {
      strncpy(schedule[day][period].name, class_name, sizeof(schedule[day][period].name));
    }
    
    // Update display
    update_schedule();
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create time layer
  time_layer = text_layer_create(GRect(0, 0, bounds.size.w, 20));
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(time_layer));
  
  // Create schedule layer
  schedule_layer = text_layer_create(GRect(5, 22, bounds.size.w - 10, bounds.size.h - 22));
  text_layer_set_text_alignment(schedule_layer, GTextAlignmentCenter);
  text_layer_set_font(schedule_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(schedule_layer));
  
  // Initial update
  update_schedule();
}

static void window_unload(Window *window) {
  text_layer_destroy(time_layer);
  text_layer_destroy(schedule_layer);
}

static void init(void) {
  // Load saved schedule
  load_schedule();
  
  // Set up AppMessage
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(256, 256);
  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  
  // Subscribe to minute tick timer
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}