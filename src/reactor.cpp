#include <math.h>
#include <string.h>
#include <time.h>

#include "tusb.h"


#include "Adafruit_NeoPixel.hpp"

// preprocessor directives that control our behaviour 
#define NEOPIXEL_PIN 13
#define NEOPIXEL_TOTAL_LIGHTS 65
#define NEOPIXEL_NUM_COLUMNS 13
#define NEOPIXEL_NUM_ROWS 5

// Time (in seconds) between last MIDI messages and screensaver activation.
#define SCREENSAVER_TIMEOUT 300
// State variables

// The time (in seconds) since the last MIDI message.  Set to the timeout by
// default so that the screen saver will display on startup.
int secondsSinceLastMIDIMessage = SCREENSAVER_TIMEOUT;

// Map of velocity by note
// We will actually only handle 0-128, but 8 bit is the closest native structure.
uint8_t MIDI_notes_held[128];

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEOPIXEL_TOTAL_LIGHTS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Timer callback
bool updateClock(struct repeating_timer *t) {
    secondsSinceLastMIDIMessage++;

    (void) t;
    return true;
}

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

    if (type == MIDI_CIN_NOTE_OFF || type == MIDI_CIN_NOTE_ON) {
        // We've received a MIDI note message, so reset the clock.
        secondsSinceLastMIDIMessage = 0;

        int note = incoming_packet[2];
        int velocity = (type == MIDI_CIN_NOTE_ON) ? incoming_packet[3] : 0;

        MIDI_notes_held[note] = velocity;
    }
  }
}

void display_screen_saver() {
    // Oscillate between darkness and light every 512 cycles.
    int brightness_seed = secondsSinceLastMIDIMessage % 512;
    int brightness = brightness_seed < 256 ? brightness_seed : 511 - brightness_seed;

    uint32_t current_colour = pixels.Color(brightness, brightness, brightness);

    pixels.fill(current_colour, 0, NEOPIXEL_TOTAL_LIGHTS);
    pixels.show();

    // TODO: Do something more interesting? 
}

void update_single_cell (uint32_t *grid, int index, uint8_t red_to_add, uint8_t green_to_add, uint8_t blue_to_add) {
    uint8_t current_red = (grid[index] >> 16) & 0xFF;
    uint8_t current_green = (grid[index] >> 8) & 0xFF;
    uint8_t current_blue = grid[index] & 0xFF;

    uint8_t new_red = (uint8_t)(fmin(current_red + red_to_add, 255));
    uint8_t new_green = (uint8_t)(fmin(current_green + green_to_add, 255));
    uint8_t new_blue = (uint8_t)(fmin(current_blue + blue_to_add, 255));

    uint32_t new_colour = ((new_red & 0xff) << 16) | ((new_green & 0xff) << 8) | (new_blue & 0xff);

    grid[index] = new_colour;
}

void update_midi_visualisation (uint32_t *grid) {
   
    for (int note = 0; note < 128; note++) {
        int velocity = grid[note];

        if (velocity) {
            // Each "note" is a "column" wide, but is not usually displayed in a single
            // column. Instead its energy is split between the nearest two columns in
            // proportion to how close it is to each.
            float raw_column = ((note/127.0) * NEOPIXEL_NUM_COLUMNS);
            int first_column = floor(raw_column);
            int second_column_percentage = (int)(raw_column * 100) % 100;
            int first_column_percentage = 100 - second_column_percentage;

            // The velocity for each note controls how many rows it fills up from bottom
            // to top, and again, it's in proportion, so 128 fills all cells in the
            // column, but 125 fills the top cell in the column a slight percentage
            // less.
            float raw_top_row = velocity / (127.0 / NEOPIXEL_NUM_ROWS);
            int top_full_row = floor(raw_top_row);
            int top_row_percentage = (int)(raw_top_row * 100) % 100;
            
            // Each note's colour is determined by its pitch, with a proportion of
            // red/green/blue spanning the full range of pitches.
            uint8_t note_red = (note <= 60) ? 255 * note / 60 : 0; // Covers the note range 0-60
            uint8_t note_blue = (note >= 40 && note <= 100) ? 255 * (note - 40) / 60: 0; // Covers the note range 40-100
            uint8_t note_green = (note >= 60) ? 255 * (note - 60) / 60: 0; // Covers the note range 60-120


            for (int row = 0; row < top_full_row; row++) {
                int first_column_index = (row * NEOPIXEL_NUM_COLUMNS) + first_column;
                update_single_cell(grid, first_column_index,
                    (note_red * first_column_percentage)/100,
                    (note_green * first_column_percentage)/100,
                    (note_blue * first_column_percentage)/100
                );

                if (second_column_percentage) {
                    int second_column_index = first_column_index + 1;
                    update_single_cell(grid, second_column_index,
                        (note_red * second_column_percentage)/100,
                        (note_green * second_column_percentage)/100,
                        (note_blue * second_column_percentage)/100
                    );
                }
            }

            // Paint the additional fraction row/cell if we have one.
            if (top_row_percentage) {
                int first_column_index = ((top_full_row + 1) * NEOPIXEL_NUM_COLUMNS) + first_column;
                update_single_cell(grid, first_column_index,
                    (note_red * top_row_percentage)/100,
                    (note_green * top_row_percentage)/100,
                    (note_blue * top_row_percentage)/100
                );

                if (second_column_percentage) {
                    int second_column_index = first_column_index + 1;
                    update_single_cell(grid, second_column_index,
                        (note_red * top_row_percentage * second_column_percentage)/10000,
                        (note_green * top_row_percentage * second_column_percentage)/10000,
                        (note_blue * top_row_percentage * second_column_percentage)/10000
                    );
                }
            }
        }
    }
}

void display_midi_visualisation(uint32_t *grid) {
    for (int index = 0; index < NEOPIXEL_TOTAL_LIGHTS; index++) {
        pixels.setPixelColor(index, grid[index]);
    }
    pixels.show();
}


int main() {
  // default 125MHz is not appropreate. Sysclock should be multiple of 12MHz.
  set_sys_clock_khz(120000, true);

  // Seemingly necessary...
  sleep_ms(10);

  // device stack on native USB
  tud_init(0);

  // Timer to keep track of timeout for screensaver
  struct repeating_timer clock_timer;
  add_repeating_timer_ms(1000, updateClock, NULL, &clock_timer);

  pixels.begin();
  pixels.clear();
  pixels.show();

  while (true) {
    tud_task();
    midi_client_task();

    if (secondsSinceLastMIDIMessage < SCREENSAVER_TIMEOUT) {
        uint32_t grid_colours[NEOPIXEL_TOTAL_LIGHTS] = { 0 };
        update_midi_visualisation(grid_colours);
        display_midi_visualisation(grid_colours);
    }
    else {
        display_screen_saver();
    }
  }
}
