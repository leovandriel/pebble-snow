//
//  snow.c
//  Snow Watchface
//
//  Copyright (c) 2013 Leonard van Driel. All rights reserved.
//

#include <pebble.h>

#define NUM_FLAKES 3000   // max 10 000
#define UPDATE_MS 50      // flakes update interval
#define SPEED 3/2         // simulation speed

#define UPDATE_S 60       // time update interval

static Window *window;
static Layer *layer;
static AppTimer *timer;

static uint16_t flakes[NUM_FLAKES];
static bool do_shake = false;
static time_t last_time = 0;

static GColor get_pixel(GContext *ctx, uint16_t offset) {
  // TODO: add additional offset if layer is not fullscreen (STRIDE * origin.h + origin.w)
  unsigned char *ptr = (unsigned char *)(((GBitmap *)ctx)->addr);
  return ((ptr[offset / 8] >> (offset % 8)) & 1) ? GColorWhite : GColorBlack;
}

static void timer_callback(void *context) {
  layer_mark_dirty(layer);
  timer = NULL;
}

static void layer_update_callback(Layer *me, GContext* ctx) {

  // preparations
  GRect bounds = layer_get_bounds(me);
  uint16_t width = bounds.size.w;
  uint16_t height = bounds.size.h;
  uint16_t stride = (bounds.size.w + 31) / 32 * 32;
  uint16_t max = (height - 1) * stride + width;
  uint16_t shake = stride - width;
  uint16_t shake_stride = shake * stride;

  // handle shake
  if (do_shake) {
    do_shake = false;
    light_enable_interaction();
    for (uint16_t i = 0, j = rand(); i < NUM_FLAKES; i++, j+=31) {
      for (uint16_t k = 0; k < 2; k++, j+=31) {
        uint16_t next = flakes[i] + j % (max * 2) - max;
        if (next < max && next % stride < width && get_pixel(ctx, next) == GColorBlack) {
          flakes[i] = next;
          break;
        }
      }
    }
    last_time = 0;
  }

  // update time text
  time_t t = time(NULL);
  if (t / UPDATE_S > last_time) {
    last_time = t / UPDATE_S;
    char time_text[6];
    clock_copy_time_string(time_text, sizeof(time_text));

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    GRect rect = (GRect) {{0, 60}, {width, 50}};
    GFont font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
    graphics_draw_text(ctx, time_text, font, rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    graphics_context_set_stroke_color(ctx, GColorWhite);
    for (uint16_t i = 0, j = rand(); i < NUM_FLAKES; i++) {
      if (get_pixel(ctx, flakes[i]) == GColorBlack) {
        graphics_draw_pixel(ctx, GPoint(flakes[i] % stride, flakes[i] / stride));
      } else {
        for (uint16_t k = 0; k < 8; k++, j++) {
          uint16_t next = flakes[i] + (j % 9 / 3 - 1) * shake_stride + (j % 3 - 1) * shake;
          if (next < max && next % stride < width && get_pixel(ctx, next) == GColorBlack) {
            flakes[i] = next;
            graphics_draw_pixel(ctx, GPoint(flakes[i] % stride, flakes[i] / stride));
            break;
          }
        }
      }
    }
  }

  // apply physics
  AccelData accel = {.x = 0, .y = 0, .z = 0};
  accel_service_peek(&accel);
  uint16_t absx = accel.x < 0 ? -accel.x : accel.x;
  uint16_t absy = accel.y < 0 ? -accel.y : accel.y;
  uint16_t span = (absx + absy + 10) * SPEED;

  for (uint16_t i = 0, j = rand(), k = rand(), l = rand(); i < span; i++, j++, k++, l++) {
    uint16_t index = j % NUM_FLAKES;
    uint16_t next = flakes[index];

    int16_t sideway = k % 3 == 0 ? l % 5 - 2 : 0;
    int16_t accx = accel.x + accel.y * sideway;
    int16_t accy = accel.y - accel.x * sideway;
    absx = accx < 0 ? -accx : accx;
    absy = accy < 0 ? -accy : accy;

    if (absx > absy || k % absy < absx) {
      if (accx > 0) {
        next++;
      } else {
        next--;
      }
    }
    if (absy > absx || l % absx < absy) {
      if (accy > 0) {
        next -= stride;
      } else {
        next += stride;
      }
    }
    if (next < max && next % stride < width && get_pixel(ctx, next) == GColorBlack) {
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_pixel(ctx, GPoint(flakes[index] % stride, flakes[index] / stride));
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_pixel(ctx, GPoint(next % stride, next / stride));
      flakes[index] = next;
    }
  }

  if (!timer) timer = app_timer_register(UPDATE_MS, timer_callback, NULL);
}

static void handle_accel(AccelData *accel_data, uint32_t num_samples) {
  // or else I will crash
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  do_shake = true;
}

static void root_update_callback(Layer *me, GContext* ctx) {
  // hack to prevent screen cleaning
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  layer_set_update_proc(window_layer, root_update_callback);
  GRect bounds = layer_get_bounds(window_layer);

  layer = layer_create(bounds);
  layer_set_update_proc(layer, layer_update_callback);
  layer_add_child(window_layer, layer);

  uint16_t width = bounds.size.w;
  uint16_t height = bounds.size.h;
  uint16_t stride = (bounds.size.w + 31) / 32 * 32;
  for (uint16_t i = 0; i < NUM_FLAKES; i++) {
    flakes[i] = rand() % height * stride + rand() % width;
  }
}

static void window_unload(Window *window) {
  layer_remove_from_parent(layer);
  layer_destroy(layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true);
  accel_data_service_subscribe(0, handle_accel);
  accel_tap_service_subscribe(&accel_tap_handler);
}

static void deinit(void) {
  app_timer_cancel(timer);
  accel_tap_service_unsubscribe();
  accel_data_service_unsubscribe();
  window_stack_remove(window, false);
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
