#include <math.h>
#include <string.h>
#include <time.h>

#include "tusb.h"

#include "pico/stdlib.h"

#include "Adafruit_NeoPixel.hpp"

// preprocessor directives that control our behaviour 
#define NEOPIXEL_PIN 15
#define NEOPIXEL_TOTAL_LIGHTS 65
#define NEOPIXEL_NUM_COLUMNS 13
#define NEOPIXEL_NUM_ROWS 5

// Corresponds to the default tuning of the Akai Pro MAX49
#define MIDI_BOTTOM_NOTE 36
#define MIDI_TOP_NOTE 84

// Time (in seconds) between last MIDI messages and screensaver activation.
#define MS_PER_TICK   200
#define REDRAW_TICKS  1
#define TIMEOUT_TICKS 5
#define SCREENSAVER_TIMEOUT_S 300
// State variables

// We can only have one timer callback running, so we use "ticks" to decide how
// to run other stuff.
int ticks = 0;

// The time (in seconds) since the last MIDI message.  Set to the timeout by
// default so that the screen saver will display on startup.
int seconds_since_last_MIDI_message = SCREENSAVER_TIMEOUT_S;

// Map of velocity by note
// We will actually only handle 0-128, but 8 bit is the closest native structure.
uint8_t global_notes_held[128];

// The string of lights we control.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEOPIXEL_TOTAL_LIGHTS, NEOPIXEL_PIN, NEO_BGR + NEO_KHZ800);

//--------------------------------------------------------------------+
// TinyUSB Client Callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {}

// Invoked when device is unmounted
void tud_umount_cb(void) {}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {}

// End TinyUSB Callbacks

//--------------------------------------------------------------------+
// MIDI Tasks
//--------------------------------------------------------------------+
void midi_client_task(void)
{
  // Read any incoming messages from our primary USB port.
  while (tud_midi_available()) {
    uint8_t incoming_packet[4];
    tud_midi_packet_read(incoming_packet);

    // The first byte is unique to USB MIDI.  The last three are common to other
    // MIDI, i.e. the first byte is status, and for notes, the second is note,
    // the third is velocity.

    int type = incoming_packet[1] >> 4;

    if (type == MIDI_CIN_NOTE_OFF || type == MIDI_CIN_NOTE_ON || type == MIDI_CIN_POLY_KEYPRESS) {
        // We've received a MIDI note message, so reset the clock.
        seconds_since_last_MIDI_message = 0;

        int note = incoming_packet[2];
        int velocity = (type == MIDI_CIN_NOTE_OFF) ? 0 : incoming_packet[3];

        global_notes_held[note] = velocity;
    }
  }
}

// This approach may bite us if we start working with a wider range of lights.
uint32_t get_colour(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t colour = ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
    return colour;
}

void display_screen_saver() {
    // Cycle each channel at different rates.
    int red = (ticks % 256);
    int green = (ticks % 128) * 2;
    int blue = (ticks % 512) / 2;


    // This seems to not correctly account for the channel order, so we use our
    // own function.
    //
    // uint32_t current_colour = pixels.Color(0, 0, brightness);

    uint32_t current_colour = get_colour(red, green, blue);

    pixels.fill(current_colour, 0, NEOPIXEL_TOTAL_LIGHTS);
    pixels.show();
}

void update_single_cell (uint32_t *grid, int index, uint8_t red_to_add, uint8_t green_to_add, uint8_t blue_to_add) {
    uint8_t current_red = (grid[index] >> 16) & 0xFF;
    uint8_t current_green = (grid[index] >> 8) & 0xFF;
    uint8_t current_blue = grid[index] & 0xFF;

    uint8_t new_red = (uint8_t)(fmin(current_red + red_to_add, 255));
    uint8_t new_green = (uint8_t)(fmin(current_green + green_to_add, 255));
    uint8_t new_blue = (uint8_t)(fmin(current_blue + blue_to_add, 255));

    uint32_t new_colour = get_colour(new_red, new_green, new_blue);

    grid[index] = new_colour;
}

void update_midi_visualisation (uint32_t *grid, uint8_t *notes_held) {
   
    for (int note = MIDI_BOTTOM_NOTE; note <= MIDI_TOP_NOTE; note++) {
        int velocity = notes_held[note];

        if (velocity) {
            // display each note on the nearest column.
            int column;
            if (note <= MIDI_BOTTOM_NOTE) {
                column = 0;
            }
            else if (note >= MIDI_TOP_NOTE) {
                column = (NEOPIXEL_NUM_COLUMNS - 1);
            }
            else {
                int note_range_pct = 100 * (note - MIDI_BOTTOM_NOTE) / (MIDI_TOP_NOTE - MIDI_BOTTOM_NOTE);
                column = ((NEOPIXEL_NUM_COLUMNS * note_range_pct) / 100) - 1;
            }

            // Note velocity controls how many rows are filled from bottom to
            // top. TODO: Make this fill from the centre out?
            int bars = velocity / (127.0 / NEOPIXEL_NUM_ROWS);
            
            // Each note's colour is determined by its pitch, with a proportion of
            // red/green/blue spanning the full range of pitches.
            uint8_t note_red = (note <= 60) ? 255 * note / 60 : 0; // Covers the note range 0-60
            uint8_t note_blue = (note >= 40 && note <= 100) ? 255 * (note - 40) / 60: 0; // Covers the note range 40-100
            uint8_t note_green = (note >= 60) ? 255 * (note - 60) / 60: 0; // Covers the note range 60-120

            for (int row = NEOPIXEL_NUM_ROWS - 1; row >= NEOPIXEL_NUM_ROWS - bars; row--) {
                int column_index = (row * NEOPIXEL_NUM_COLUMNS) + column;
                update_single_cell(grid, column_index, note_red, note_green, note_blue);
            }
        }
    }
}

void display_midi_visualisation(uint32_t *grid) {
    pixels.clear();

    for (int row = 0; row < NEOPIXEL_NUM_ROWS; row++) {
        bool isReversed = (row % 2) == 0;
        for (int column = 0; column < NEOPIXEL_NUM_COLUMNS; column++) {
            int pixel_index = (row * NEOPIXEL_NUM_COLUMNS) + column;
            int grid_column_index = isReversed ? (NEOPIXEL_NUM_COLUMNS - 1) - column : column;
            int grid_index = (row * NEOPIXEL_NUM_COLUMNS) + grid_column_index;

            pixels.setPixelColor(pixel_index, grid[grid_index]);
        }
    }

    pixels.show();
}

// Timer callback
void redraw() {
    if (seconds_since_last_MIDI_message < SCREENSAVER_TIMEOUT_S) {
        uint32_t grid_colours[NEOPIXEL_TOTAL_LIGHTS] = { 0 };
        update_midi_visualisation(grid_colours, global_notes_held);
        display_midi_visualisation(grid_colours);
    }
    else {
        display_screen_saver();
    }
}

bool handle_single_tick(struct repeating_timer *t) {
    ticks++;

    if ((ticks % REDRAW_TICKS) == 0) {
        redraw();
    }

    if ((ticks % TIMEOUT_TICKS) == 0) {
        seconds_since_last_MIDI_message++;
    }

    (void) t;
    return true;
}

// Startup animation, light each row and column in turn.
void startup_animation () {
  uint32_t test_colour = get_colour(30, 30, 30);

  // The easy part, test each row.
  for (int row = 0; row < NEOPIXEL_NUM_ROWS; row++) {
    int light_index = (row * NEOPIXEL_NUM_COLUMNS);
    pixels.clear();

    pixels.fill(test_colour, light_index, NEOPIXEL_NUM_COLUMNS);
    pixels.show();
    sleep_ms(250);
  }

  // The less easy part, test each column, taking into account that, since we
  // used a single string of lights rather than a curtain, every other row is reversed.

  for (int column = 0; column < NEOPIXEL_NUM_COLUMNS; column++) {
    pixels.clear();
    for (int row = 0; row < NEOPIXEL_NUM_ROWS; row++) {
        bool isReversed = (row % 2) == 0;
        int column_index = isReversed ? (NEOPIXEL_NUM_COLUMNS - 1) - column : column;
        int pixel_index = (row * NEOPIXEL_NUM_COLUMNS) + column_index;
        pixels.setPixelColor(pixel_index, test_colour);
    }
    pixels.show();
    sleep_ms(250);
  }

  sleep_ms(250);
}

int main() {
  // default 125MHz is not appropreate. Sysclock should be multiple of 12MHz.
  set_sys_clock_khz(120000, true);

  // Seemingly necessary...
  sleep_ms(10);

  pixels.begin();

  startup_animation();

  // We need to handle MIDI messages in real time, everything else we attempt
  // periodically via a timed callback. It seems like we can only have one, so
  // we count "ticks" and run everything else based on the tick count.

  struct repeating_timer tick_timer;
  add_repeating_timer_ms(MS_PER_TICK, handle_single_tick, NULL, &tick_timer);

  // device stack on native USB
  tud_init(0);

  stdio_init_all();

  while (true) {
    tud_task();
    midi_client_task();
  }
}
