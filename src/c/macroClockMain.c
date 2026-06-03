#include "pebble.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define M_PI 3.14

// Persistent storage key for our settings struct
#define SETTINGS_KEY 1

// Height of the date / digital-time bars at the top and bottom of the screen
#define DATE_BAR_HEIGHT 24

// This defines graphics path information to be loaded as a path later
static const GPathInfo LINE_PATH_POINTS = {
  // This is the amount of points
  4,
  // A path can be concave, but it should not twist on itself
  // The points should be defined in clockwise order due to the rendering
  // implementation. Counter-clockwise will work in older firmwares, but
  // it is not officially supported
  // Long enough to span the diagonal of the largest supported display
  // (emery 200x228 -> ~152px half-diagonal). The hand is clipped to the layer
  // bounds, so an oversized length is harmless on smaller screens like basalt.
  (GPoint []) {
    {-4, 170},
    {4, 170},
    {4, -170},
	  {-4, -170}
  }
};

enum DateToggle {
	DT_OFF = 0,
	DT_FLICK = 1,
	DT_ALWAYS_ON = 2
};

// The settings managed by Clay and synced from the configuration page.
// Persisted as a single blob under SETTINGS_KEY (see prv_load/save_settings).
typedef struct ClaySettings {
	GColor backgroundColor;
	GColor handColor;
	GColor handBorderColor;
	GColor dotColor;
	GColor hourColor;
	bool handBorderToggle;
	bool hourFormat;
	int dateToggle; //0 = Off, 1 = Flick, 2 = Always On
	int digTimeToggle;
} ClaySettings;

// Layout centre points, derived from the root layer bounds at window load so
// the watchface adapts to the display size (basalt 144x168, emery 200x228).
static int midWidth;
static int midHeight;
static int screenMidWidth;
static int screenMidHeight;
static const int radius = 250;
const int clockUnit = 30;

static Window *s_main_window;

static TextLayer * s_time_layer;
static TextLayer * s_time_layer2;
static TextLayer * s_date_layer;
static TextLayer * s_date_layer2;

static Layer * s_path_layer;
static GPath * s_line_path;

static Layer * topPathLayer;
static Layer * botPathLayer;

static Layer * timeLayer;
static Layer * timeLayer2;
static Layer * dateLayer;
static Layer * dateLayer2;

static Layer * s_dot_layer;

static int s_path_angle;
static double s_path_angle_adj_rad;
static int s_hour_angle;
static double s_hour_angle_adj_rad;

static char buffer[2];
static char buffer2[2];
static char dateBuffer[16];
static char dateBuffer2[9];

static ClaySettings settings;

static double getCos(double angle) {
	return ( (double) cos_lookup(angle * TRIG_MAX_ANGLE / (2 * M_PI)) / (double) TRIG_MAX_RATIO);
}

static double getSin(double angle) {
	return ( (double) sin_lookup(angle * TRIG_MAX_ANGLE / (2 * M_PI)) / (double) TRIG_MAX_RATIO);
}

static GColor getColor(char* colorString) {
	if (strcmp(colorString, "blk") == 0) {
		return GColorBlack;
	}
	else if (strcmp(colorString, "wht") == 0) {
		return GColorWhite;
	}
	else if (strcmp(colorString, "red") == 0) {
		return GColorRed;
	}
	else if (strcmp(colorString, "org") == 0) {
		return GColorOrange;
	}
	else if (strcmp(colorString, "ylw") == 0) {
		return GColorYellow;
	}
	else if (strcmp(colorString, "grn") == 0) {
		return GColorDarkGreen;
	}
	else if (strcmp(colorString, "ble") == 0) {
		return GColorDukeBlue;
	}
	else if (strcmp(colorString, "prp") == 0) {
		return GColorImperialPurple;
	}
	else if (strcmp(colorString, "pnk") == 0) {
		return GColorShockingPink;
	}
	else if (strcmp(colorString, "gry") == 0) {
		return GColorDarkGray;
	}
	else {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "getColor received an invalid string: %s", colorString);
		return GColorWhite;
	}
}

static int getToggle(char* toggleString) {
	if (strcmp(toggleString, "flick") == 0) {
		return DT_FLICK;
	}
	else if (strcmp(toggleString, "on") == 0) {
		return DT_ALWAYS_ON;
	}
	else {
		return DT_OFF;
	}
}

void in_dropped_handler(AppMessageResult reason, void *ctx) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message Dropped: %d", reason);
}

static void top_line_layer_update_callback(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	graphics_context_set_stroke_color(ctx, settings.handColor);
	graphics_draw_line(ctx, GPoint(0, DATE_BAR_HEIGHT), GPoint(bounds.size.w, DATE_BAR_HEIGHT));
}

static void bot_line_layer_update_callback(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	int y = bounds.size.h - DATE_BAR_HEIGHT - 1;
	graphics_context_set_stroke_color(ctx, settings.handColor);
	graphics_draw_line(ctx, GPoint(0, y), GPoint(bounds.size.w, y));
}

// Layer update callback which is called on render updates
static void path_layer_update_callback(Layer *layer, GContext *ctx) {
	// Getting the current time
	time_t tempTime = time(NULL);
	struct tm * timeStruct = localtime(&tempTime);

	s_path_angle = (((timeStruct->tm_hour % 12) * 60) + timeStruct->tm_min) * 360 / (12 * 60);

	gpath_rotate_to(s_line_path, (TRIG_MAX_ANGLE / 360) * s_path_angle);

	graphics_context_set_stroke_color(ctx, settings.handBorderColor);
	graphics_context_set_fill_color(ctx, settings.handColor);
	gpath_draw_filled(ctx, s_line_path);
	if (settings.handBorderToggle) {
		gpath_draw_outline(ctx, s_line_path);
	}
}

static void dot_layer_update_callback(Layer *layer, GContext *ctx) {

	graphics_context_set_stroke_color(ctx, settings.dotColor);
	graphics_context_set_fill_color(ctx, settings.dotColor);

	time_t tempTime = time(NULL);
	struct tm * tick_time = localtime(&tempTime);

	s_path_angle = (((tick_time->tm_hour % 12) * 60) + tick_time->tm_min) / 2;
	s_path_angle_adj_rad = -(s_path_angle * M_PI / 180) + (M_PI / 2);

	s_hour_angle = (((tick_time->tm_hour % 12) * 60)) / 2;
	s_hour_angle_adj_rad = -(s_hour_angle * M_PI / 180) + (M_PI / 2);

	if (s_path_angle_adj_rad > (M_PI / 2) &&
	   s_path_angle_adj_rad < (3 * M_PI / 2)) {
		s_path_angle_adj_rad += (2 * M_PI);
	}

	if (s_hour_angle_adj_rad > (M_PI / 2) &&
	   s_hour_angle_adj_rad < (3 * M_PI / 2)) {
		s_hour_angle_adj_rad += (2 * M_PI);
	}

	double timeX = getCos(s_path_angle_adj_rad) * radius;
	double timeY = getSin(s_path_angle_adj_rad) * radius;

	double preDotX = getCos(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (M_PI / 24))) * radius;
	double preDotX2 = getCos(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double preDotX3 = getCos(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (3 * M_PI / 24))) * radius;

	double preDotY = getSin(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (M_PI / 24))) * radius;
	double preDotY2 = getSin(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double preDotY3 = getSin(s_hour_angle_adj_rad - ((0 * M_PI / 6) - (3 * M_PI / 24))) * radius;

	double postDotX = getCos(s_hour_angle_adj_rad - ((M_PI / 6) - (M_PI / 24))) * radius;
	double postDotX2 = getCos(s_hour_angle_adj_rad - ((M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postDotX3 = getCos(s_hour_angle_adj_rad - ((M_PI / 6) - (3 * M_PI / 24))) * radius;

	double postDotY = getSin(s_hour_angle_adj_rad - ((M_PI / 6) - (M_PI / 24))) * radius;
	double postDotY2 = getSin(s_hour_angle_adj_rad - ((M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postDotY3 = getSin(s_hour_angle_adj_rad - ((M_PI / 6) - (3 * M_PI / 24))) * radius;
	double postPostDotX = getCos(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (M_PI / 24))) * radius;
	double postPostDotX2 = getCos(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postPostDotX3 = getCos(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (3 * M_PI / 24))) * radius;
	double postPostDotY = getSin(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (M_PI / 24))) * radius;
	double postPostDotY2 = getSin(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (2 * M_PI / 24))) * radius;
	double postPostDotY3 = getSin(s_hour_angle_adj_rad - ((2 * M_PI / 6) - (3 * M_PI / 24))) * radius;

	struct GPoint preDot1,
		preDot2,
		preDot3,
		postDot1,
		postDot2,
		postDot3,
		postPostDot1,
		postPostDot2,
		postPostDot3;

	preDot1.x = (preDotX - timeX) + screenMidWidth;
	preDot1.y = (timeY - preDotY) + screenMidHeight;

	preDot2.x = (preDotX2 - timeX) + screenMidWidth;
	preDot2.y = (timeY - preDotY2) + screenMidHeight;

	preDot3.x = (preDotX3 - timeX) + screenMidWidth;
	preDot3.y = (timeY - preDotY3) + screenMidHeight;

	postDot1.x = (postDotX - timeX) + screenMidWidth;
	postDot1.y = (timeY - postDotY) + screenMidHeight;

	postDot2.x = (postDotX2 - timeX) + screenMidWidth;
	postDot2.y = (timeY - postDotY2) + screenMidHeight;

	postDot3.x = (postDotX3 - timeX) + screenMidWidth;
	postDot3.y = (timeY - postDotY3) + screenMidHeight;

	postPostDot1.x = (postPostDotX - timeX) + screenMidWidth;
	postPostDot1.y = (timeY - postPostDotY) + screenMidHeight;

	postPostDot2.x = (postPostDotX2 - timeX) + screenMidWidth;
	postPostDot2.y = (timeY - postPostDotY2) + screenMidHeight;

	postPostDot3.x = (postPostDotX3 - timeX) + screenMidWidth;
	postPostDot3.y = (timeY - postPostDotY3) + screenMidHeight;

	graphics_fill_circle(ctx, preDot1, 3);
	graphics_fill_circle(ctx, preDot2, 5);
	graphics_fill_circle(ctx, preDot3, 3);
	graphics_fill_circle(ctx, postDot1, 3);
	graphics_fill_circle(ctx, postDot2, 5);
	graphics_fill_circle(ctx, postDot3, 3);
	graphics_fill_circle(ctx, postPostDot1, 3);
	graphics_fill_circle(ctx, postPostDot2, 5);
	graphics_fill_circle(ctx, postPostDot3, 3);
}

static void update_time() {
	time_t tempTime = time(NULL);
	struct tm * tick_time = localtime(&tempTime);
	int currHour = tick_time->tm_hour;

	strftime(dateBuffer, sizeof(dateBuffer), "%a, %b %e", tick_time);

	if (settings.hourFormat) {
		strftime(buffer, sizeof("00"), "%H", tick_time);
		strftime(dateBuffer2, sizeof("00:00"), "%H:%M", tick_time);
	}
	else {
		strftime(buffer, sizeof("00"), "%I", tick_time);
		strftime(dateBuffer2, sizeof("00:00 XX"), "%l:%M %p", tick_time);
	}

	s_path_angle = (((tick_time->tm_hour % 12) * 60) + tick_time->tm_min) / 2;
	s_path_angle_adj_rad = -(s_path_angle * M_PI / 180) + (M_PI / 2);

	s_hour_angle = (((tick_time->tm_hour % 12) * 60)) / 2;
	s_hour_angle_adj_rad = -(s_hour_angle * M_PI / 180) + (M_PI / 2);

	if (s_path_angle_adj_rad > (M_PI / 2) &&
	   s_path_angle_adj_rad < (3 * M_PI / 2)) {
		s_path_angle_adj_rad += (2 * M_PI);
	}

	if (s_hour_angle_adj_rad > (M_PI / 2) &&
	   s_hour_angle_adj_rad < (3 * M_PI / 2)) {
		s_hour_angle_adj_rad += (2 * M_PI);
	}

	if (tick_time->tm_hour == 23) {
		tick_time->tm_hour = 0;
	}
	else {
		tick_time->tm_hour++;
	}

	if (settings.hourFormat) {
		strftime(buffer2, sizeof("00"), "%H", tick_time);
	}
	else {
		strftime(buffer2, sizeof("00"), "%I", tick_time);
	}

	double timeX = getCos(s_path_angle_adj_rad) * radius;
	double timeY = getSin(s_path_angle_adj_rad) * radius;
	double hourX = getCos(s_hour_angle_adj_rad) * radius;
	double hourY = getSin(s_hour_angle_adj_rad) * radius;

	double hourX2 = getCos(s_hour_angle_adj_rad - (0.5236)) * radius;
	double hourY2 = getSin(s_hour_angle_adj_rad - (0.5236)) * radius;

	int xPos = -(timeX - hourX) + midWidth;
	int yPos = -(hourY - timeY) + midHeight;

	int xPos2 = -(timeX - hourX2) + midWidth;
	int yPos2 = -(hourY2 - timeY) + midHeight;

	//xPos-5 and width=60 because "20" doesn't fit in 50x50 apparently.
	layer_set_frame(timeLayer, GRect(xPos-5,yPos,60,50));
	layer_set_frame(timeLayer2, GRect(xPos2-5,yPos2,60,50));

	char * bufferS = buffer+1;
	char * buffer2S = buffer2+1;

	if (settings.hourFormat) {
		if (currHour > 9) {
			text_layer_set_text(s_time_layer, buffer);
			if (currHour == 23) {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2);
			}
		}
		else {
			text_layer_set_text(s_time_layer, bufferS);

			if (currHour == 9) {
				text_layer_set_text(s_time_layer2, buffer2);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
		}
	}
	else {
		if (currHour > 0 && currHour < 10) {
			text_layer_set_text(s_time_layer, bufferS);
		}
		else if (currHour > 12 && currHour < 22) {
			text_layer_set_text(s_time_layer, bufferS);
		}
		else {
			text_layer_set_text(s_time_layer, buffer);
		}

		if (currHour >= 0 && currHour < 9) {
			text_layer_set_text(s_time_layer2, buffer2S);
		}
		else if (currHour >= 12 && currHour < 21) {
			text_layer_set_text(s_time_layer2, buffer2S);
		}
		else {
			text_layer_set_text(s_time_layer2, buffer2);
		}
	}

	text_layer_set_text(s_date_layer, dateBuffer);
	text_layer_set_text(s_date_layer2, dateBuffer2);

	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(dateLayer);
	layer_mark_dirty(dateLayer2);

	gpath_rotate_to(s_line_path, (TRIG_MAX_ANGLE / 360) * s_path_angle);
	layer_mark_dirty(s_path_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	update_time();
}

static void hideDate(void *data) {
	layer_set_hidden(data, true);
}


static void tap_handler(AccelAxisType axis, int32_t direction) {
	if (settings.dateToggle == DT_FLICK) {
		layer_set_hidden(dateLayer, false);
		layer_set_hidden(topPathLayer, false);
		app_timer_register(3500, hideDate, dateLayer);
		app_timer_register(3500, hideDate, topPathLayer);
	}

	if (settings.digTimeToggle == DT_FLICK) {
		layer_set_hidden(dateLayer2, false);
		layer_set_hidden(botPathLayer, false);
		app_timer_register(3500, hideDate, dateLayer2);
		app_timer_register(3500, hideDate, botPathLayer);
	}
}

// Initialise the settings to their config.js default values
static void prv_default_settings() {
	settings.backgroundColor = GColorWhite;
	settings.handColor = GColorRed;
	settings.handBorderColor = GColorDarkGray;
	settings.dotColor = GColorDarkGray;
	settings.hourColor = GColorBlack;
	settings.handBorderToggle = false;
	settings.hourFormat = true;
	settings.dateToggle = DT_FLICK;
	settings.digTimeToggle = DT_OFF;
}

// Read settings from persistent storage, falling back to the defaults
static void prv_load_settings() {
	prv_default_settings();
	persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

// Apply the current settings to the display
static void prv_update_display() {
	if (settings.dateToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer, false);
		layer_set_hidden(topPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer, true);
		layer_set_hidden(topPathLayer, true);
	}

	if (settings.digTimeToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer2, false);
		layer_set_hidden(botPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer2, true);
		layer_set_hidden(botPathLayer, true);
	}

	window_set_background_color(s_main_window, settings.backgroundColor);
	text_layer_set_background_color(s_time_layer, settings.backgroundColor);
	text_layer_set_background_color(s_time_layer2, settings.backgroundColor);
	text_layer_set_background_color(s_date_layer, settings.backgroundColor);
	text_layer_set_background_color(s_date_layer2, settings.backgroundColor);
	text_layer_set_text_color(s_date_layer, settings.hourColor);
	text_layer_set_text_color(s_date_layer2, settings.hourColor);
	text_layer_set_text_color(s_time_layer, settings.hourColor);
	text_layer_set_text_color(s_time_layer2, settings.hourColor);

	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(s_dot_layer);
	layer_mark_dirty(s_path_layer);
	layer_mark_dirty(dateLayer);
	layer_mark_dirty(dateLayer2);
	layer_mark_dirty(topPathLayer);
	layer_mark_dirty(botPathLayer);
	update_time();
}

// Save the settings to persistent storage and refresh the display
static void prv_save_settings() {
	persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
	prv_update_display();
}

// Handle the config data synced from Clay on the phone
static void prv_inbox_received_handler(DictionaryIterator *iter, void *ctx) {
	Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_backgroundColor);
	if (bg_color_t) {
		settings.backgroundColor = getColor(bg_color_t->value->cstring);
	}

	Tuple *hour_color_t = dict_find(iter, MESSAGE_KEY_hourColor);
	if (hour_color_t) {
		settings.hourColor = getColor(hour_color_t->value->cstring);
	}

	Tuple *hand_color_t = dict_find(iter, MESSAGE_KEY_handColor);
	if (hand_color_t) {
		settings.handColor = getColor(hand_color_t->value->cstring);
	}

	Tuple *dot_color_t = dict_find(iter, MESSAGE_KEY_dotColor);
	if (dot_color_t) {
		settings.dotColor = getColor(dot_color_t->value->cstring);
	}

	Tuple *hand_outline_t = dict_find(iter, MESSAGE_KEY_handOutlineColor);
	if (hand_outline_t) {
		if (strcmp(hand_outline_t->value->cstring, "nob") == 0) {
			settings.handBorderToggle = false;
		}
		else {
			settings.handBorderToggle = true;
			settings.handBorderColor = getColor(hand_outline_t->value->cstring);
		}
	}

	Tuple *hour_format_t = dict_find(iter, MESSAGE_KEY_hourFormat);
	if (hour_format_t) {
		settings.hourFormat = strcmp(hour_format_t->value->cstring, "24h") == 0;
	}

	Tuple *date_toggle_t = dict_find(iter, MESSAGE_KEY_dateToggle);
	if (date_toggle_t) {
		settings.dateToggle = getToggle(date_toggle_t->value->cstring);
	}

	Tuple *dig_time_toggle_t = dict_find(iter, MESSAGE_KEY_digTimeToggle);
	if (dig_time_toggle_t) {
		settings.digTimeToggle = getToggle(dig_time_toggle_t->value->cstring);
	}

	prv_save_settings();
}

static void main_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	screenMidWidth = bounds.size.w / 2;
	screenMidHeight = bounds.size.h / 2;
	midWidth = screenMidWidth - 25;
	midHeight = screenMidHeight - 25;

	time_t tempTime = time(NULL);
	struct tm * tick_time = localtime(&tempTime);
	int currHour = tick_time->tm_hour;

	strftime(dateBuffer, sizeof(dateBuffer), "%a, %b %e", tick_time);

	if (settings.hourFormat) {
		strftime(buffer, sizeof("00"), "%H", tick_time);
		strftime(dateBuffer2, sizeof("00:00"), "%H:%M", tick_time);
	}
	else {
		strftime(buffer, sizeof("00"), "%I", tick_time);
		strftime(dateBuffer2, sizeof("00:00 XX"), "%l:%M %p", tick_time);
	}

	s_path_angle = (((tick_time->tm_hour % 12) * 60) + tick_time->tm_min) / 2;
	s_path_angle_adj_rad = -(s_path_angle * M_PI / 180) + (M_PI / 2);

	s_hour_angle = (((tick_time->tm_hour % 12) * 60)) / 2;
	s_hour_angle_adj_rad = -(s_hour_angle * M_PI / 180) + (M_PI / 2);

	if (s_path_angle_adj_rad > (M_PI / 2) &&
	   s_path_angle_adj_rad < (3 * M_PI / 2)) {
		s_path_angle_adj_rad += (2 * M_PI);
	}

	if (s_hour_angle_adj_rad > (M_PI / 2) &&
	   s_hour_angle_adj_rad < (3 * M_PI / 2)) {
		s_hour_angle_adj_rad += (2 * M_PI);
	}

	if (tick_time->tm_hour == 23) {
		tick_time->tm_hour = 0;
	}
	else {
		tick_time->tm_hour++;
	}

	if (settings.hourFormat) {
		strftime(buffer2, sizeof("00"), "%H", tick_time);
	}
	else {
		strftime(buffer2, sizeof("00"), "%I", tick_time);
	}

	double timeX = getCos(s_path_angle_adj_rad) * radius;
	double timeY = getSin(s_path_angle_adj_rad) * radius;

	double hourX = getCos(s_hour_angle_adj_rad) * radius;
	double hourY = getSin(s_hour_angle_adj_rad) * radius;

	double hourX2 = getCos(s_hour_angle_adj_rad - (M_PI / 6)) * radius;
	double hourY2 = getSin(s_hour_angle_adj_rad - (M_PI / 6)) * radius;

	int xPos = (hourX - timeX) + midWidth;
	int yPos = (timeY - hourY) + midHeight;

	int xPos2 = (hourX2 - timeX) + midWidth;
	int yPos2 = (timeY - hourY2) + midHeight;

	char * bufferS = buffer+1;
	char * buffer2S = buffer2+1;

	s_time_layer = text_layer_create(GRect(xPos-5, yPos, 60, 50));
	text_layer_set_background_color(s_time_layer, settings.backgroundColor);
	text_layer_set_text_color(s_time_layer, settings.hourColor);

	s_time_layer2 = text_layer_create(GRect(xPos2-5, yPos2, 60, 50));
	text_layer_set_background_color(s_time_layer2, settings.backgroundColor);
	text_layer_set_text_color(s_time_layer2, settings.hourColor);

	s_date_layer = text_layer_create(GRect(0, 0, bounds.size.w, DATE_BAR_HEIGHT));
	text_layer_set_background_color(s_date_layer, settings.backgroundColor);
	text_layer_set_text_color(s_date_layer, settings.hourColor);
	text_layer_set_text(s_date_layer, dateBuffer);

	s_date_layer2 = text_layer_create(GRect(0, bounds.size.h - DATE_BAR_HEIGHT, bounds.size.w, DATE_BAR_HEIGHT));
	text_layer_set_background_color(s_date_layer2, settings.backgroundColor);
	text_layer_set_text_color(s_date_layer2, settings.hourColor);
	text_layer_set_text(s_date_layer2, dateBuffer2);


	if (settings.hourFormat) {
		if (currHour > 9) {
			text_layer_set_text(s_time_layer, buffer);
			if (tick_time->tm_hour == 0) {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2);
			}
		}
		else {
			text_layer_set_text(s_time_layer, bufferS);

			if (currHour == 9) {
				text_layer_set_text(s_time_layer2, buffer2);
			}
			else {
				text_layer_set_text(s_time_layer2, buffer2S);
			}
		}
	}
	else {
		if (currHour > 0 && currHour < 10) {
			text_layer_set_text(s_time_layer, bufferS);
		}
		else if (currHour > 12 && currHour < 22) {
			text_layer_set_text(s_time_layer, bufferS);
		}
		else {
			text_layer_set_text(s_time_layer, buffer);
		}

		if (currHour >= 0 && currHour < 9) {
			text_layer_set_text(s_time_layer2, buffer2S);
		}
		else if (currHour >= 12 && currHour < 21) {
			text_layer_set_text(s_time_layer2, buffer2S);
		}
		else {
			text_layer_set_text(s_time_layer2, buffer2);
		}
	}

	text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

	text_layer_set_font(s_time_layer2, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
	text_layer_set_text_alignment(s_time_layer2, GTextAlignmentCenter);

	text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

	text_layer_set_font(s_date_layer2, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
	text_layer_set_text_alignment(s_date_layer2, GTextAlignmentCenter);

	s_dot_layer = layer_create(bounds);
	layer_set_update_proc(s_dot_layer, dot_layer_update_callback);

	s_path_layer = layer_create(bounds);
	layer_set_update_proc(s_path_layer, path_layer_update_callback);

	topPathLayer = layer_create(bounds);
	layer_set_update_proc(topPathLayer, top_line_layer_update_callback);

	botPathLayer = layer_create(bounds);
	layer_set_update_proc(botPathLayer, bot_line_layer_update_callback);

	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer2));

	layer_add_child(window_layer, s_dot_layer);
	layer_add_child(window_layer, s_path_layer);

	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer2));


	layer_add_child(window_layer, topPathLayer);
	layer_add_child(window_layer, botPathLayer);

	// Move all paths to the center of the screen
	gpath_move_to(s_line_path, GPoint(bounds.size.w/2, bounds.size.h/2));
}

static void main_window_unload(Window *window) {
	layer_destroy(s_path_layer);
	layer_destroy(s_dot_layer);
	layer_destroy(topPathLayer);
	layer_destroy(botPathLayer);
	layer_destroy(timeLayer);
	layer_destroy(timeLayer2);
	layer_destroy(dateLayer);
	layer_destroy(dateLayer2);
}

static void init() {
	s_line_path = gpath_create(&LINE_PATH_POINTS);

	prv_load_settings();

	// Create Window
	s_main_window = window_create();
	window_set_background_color(s_main_window, settings.backgroundColor);
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload,
	});

	window_stack_push(s_main_window, true);

	// Pass the corresponding GPathInfo to initialize a GPath
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	app_message_register_inbox_received(prv_inbox_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_open(128, 128);

	timeLayer = text_layer_get_layer(s_time_layer);
	timeLayer2 = text_layer_get_layer(s_time_layer2);
	dateLayer = text_layer_get_layer(s_date_layer);
	dateLayer2 = text_layer_get_layer(s_date_layer2);

	if (settings.dateToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer, false);
		layer_set_hidden(topPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer, true);
		layer_set_hidden(topPathLayer, true);
	}

	if (settings.digTimeToggle == DT_ALWAYS_ON) {
		layer_set_hidden(dateLayer2, false);
		layer_set_hidden(botPathLayer, false);
	}
	else {
		layer_set_hidden(dateLayer2, true);
		layer_set_hidden(botPathLayer, true);
	}

	layer_insert_above_sibling(topPathLayer, dateLayer);
	layer_insert_above_sibling(botPathLayer, dateLayer2);

	layer_mark_dirty(timeLayer);
	layer_mark_dirty(timeLayer2);
	layer_mark_dirty(s_dot_layer);
	layer_mark_dirty(s_path_layer);
	layer_mark_dirty(topPathLayer);
	layer_mark_dirty(botPathLayer);
	layer_mark_dirty(dateLayer);
	layer_mark_dirty(dateLayer2);

	accel_tap_service_subscribe(tap_handler);

}

static void deinit() {
	window_destroy(s_main_window);

	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	accel_tap_service_unsubscribe();

	gpath_destroy(s_line_path);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
