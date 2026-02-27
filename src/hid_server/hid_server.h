
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#ifndef hid_server_hpp
#define hid_server_hpp

#include <stdio.h>
#include "hci_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// https://w3c.github.io/gamepad/#dfn-standard-gamepad-layout <- might be useful at some point

// big report 0x37
// https://web.archive.org/web/20080630032911/http://wiki.wiimoteproject.com/Reports
// ?    X1    X0    PLUS    UP    DOWN    RIGHT    LEFT
// HOME Z1    Y1    MINUS   A     B       ONE       TWO
// x,y,z
// ir0..9
// ext0..5
// classic controller
// BDR    BDD    BLT    B-    BH    B+    BRT    1
// BZL    BB     BY     BA    BX    BZR   BDL    BDU

#if 0
// template maps
const uint32_t _common_[16] = {
    0,  // msb
    0,
    0,
    0, // PLUS
    0, // UP
    0, // DOWN
    0, // RIGHT
    0, // LEFT

    0, // HOME
    0,
    0,
    0, // MINUS
    0, // A
    0, // B
    0, // ONE
    0, // TWO
};

const uint32_t _classic_[16] = {
    0,  // RIGHT
    0,  // DOWN
    0,  // LEFT_TOP
    0,  // MINUS
    0,  // HOME
    0,  // PLUS
    0,  // RIGHT_TOP
    0,

    0,  // LOWER_LEFT
    0,  // B
    0,  // Y
    0,  // A
    0,  // X
    0,  // LOWER_RIGHT
    0,  // LEFT
    0   // UP
};
#endif

enum wii_flags {
    wiimote = 0x01,
    nunchuk = 0x02,
    classic_controller = 0x04,
    drums_guitar = 0x08,
    motion = 0x10,
    extension_read = 0x100,
    encryption_disabled = 0x200,

    // ?    XLRLSB    XLRLSB    PLUS    UP    DOWN    RIGHT    LEFT
    // HOME XLRLSB    XLRLSB    MINUS   A     B       ONE       TWO
    wii_plus = 0x1000,
    wii_up = 0x0800,
    wii_down = 0x0400,
    wii_right = 0x0200,
    wii_left = 0x0100,
    wii_home = 0x0080,
    wii_minus = 0x0010,
    wii_a = 0x0008,
    wii_b = 0x0004,
    wii_one = 0x0002,
    wii_two = 0x0001,

    // BDR    BDD    BLT    B-    BH    B+    BRT    1
    // BZL    BB     BY     BA    BX    BZR   BDL    BDU
    wii_classic_right = 0x8000,
    wii_classic_down = 0x4000,
    wii_classic_left_top = 0x2000,
    wii_classic_minus = 0x1000,

    wii_classic_home = 0x0800,
    wii_classic_plus = 0x0400,
    wii_classic_right_top = 0x0200,

    wii_classic_b = 0x0040,
    wii_classic_y = 0x0020,
    wii_classic_a = 0x0010,

    wii_classic_x = 0x0008,
    wii_classic_left = 0x0002,
    wii_classic_up = 0x0001,
};

typedef struct {
    uint32_t flags;
    uint8_t report[32];
    uint16_t common()   { return (report[2] << 8) | report[3]; }
    uint16_t classic()  {
        if (report[0] == 0x37)
            return ((report[21] << 8) | report[22]) ^ 0xFFFF;
        return ((report[8] << 8) | report[9]) ^ 0xFFFF;
    }
} wii_state;

// report all of the wii states
extern wii_state wii_states[4];
uint32_t wii_map(int index, const uint32_t* common, const uint32_t* classic);

//==================================================================
// Nintendo Switch Controller Support

// FSM states for controller initialization
enum switch_init_state {
    SWITCH_STATE_UNINIT,
    SWITCH_STATE_SETUP,
    SWITCH_STATE_REQ_DEV_INFO,
    SWITCH_STATE_READ_FACTORY_STICK_CAL,
    SWITCH_STATE_READ_USER_STICK_CAL,
    SWITCH_STATE_READ_FACTORY_IMU_CAL,
    SWITCH_STATE_SET_FULL_REPORT,
    SWITCH_STATE_ENABLE_IMU,
    SWITCH_STATE_UPDATE_LED,
    SWITCH_STATE_READY,
};

// Subcommand IDs
enum switch_subcmd {
    SWITCH_SUBCMD_REQ_DEV_INFO = 0x02,
    SWITCH_SUBCMD_SET_REPORT_MODE = 0x03,
    SWITCH_SUBCMD_SPI_FLASH_READ = 0x10,
    SWITCH_SUBCMD_SET_PLAYER_LEDS = 0x30,
    SWITCH_SUBCMD_ENABLE_IMU = 0x40,
};

// Controller types
enum switch_controller_types {
    SWITCH_CONTROLLER_TYPE_JCL = 0x01,   // Joy-con left
    SWITCH_CONTROLLER_TYPE_JCR = 0x02,   // Joy-con right
    SWITCH_CONTROLLER_TYPE_PRO = 0x03,   // Pro Controller
    SWITCH_CONTROLLER_TYPE_SNES = 0x0b,  // SNES Controller
};

// SPI Flash memory addresses for calibration data
#define SWITCH_FACTORY_STICK_CAL_ADDR_LEFT  0x603d
#define SWITCH_FACTORY_STICK_CAL_ADDR_RIGHT 0x6046
#define SWITCH_USER_STICK_CAL_ADDR_LEFT     0x8010
#define SWITCH_USER_STICK_CAL_ADDR_RIGHT    0x801B
#define SWITCH_FACTORY_IMU_CAL_ADDR         0x6020

#define SWITCH_FACTORY_STICK_CAL_SIZE       9
#define SWITCH_USER_STICK_CAL_SIZE          11
#define SWITCH_FACTORY_IMU_CAL_SIZE         24

enum switch_flags {
    switch_controller = 0x10,
    switch_joycon_left = 0x20,
    switch_joycon_right = 0x40,
    switch_pro_controller = 0x80,

    // Button flags for Switch controllers
    // Pro Controller / Combined Joy-Cons button layout
    switch_a = 0x0001,
    switch_b = 0x0002,
    switch_x = 0x0004,
    switch_y = 0x0008,
    switch_l = 0x0010,
    switch_r = 0x0020,
    switch_zl = 0x0040,
    switch_zr = 0x0080,
    switch_minus = 0x0100,
    switch_plus = 0x0200,
    switch_lstick = 0x0400,
    switch_rstick = 0x0800,
    switch_home = 0x1000,
    switch_capture = 0x2000,
    switch_up = 0x10000,
    switch_down = 0x20000,
    switch_left = 0x40000,
    switch_right = 0x80000,
};

// Calibration structure for sticks
struct switch_cal_stick {
    int32_t min;
    int32_t center;
    int32_t max;
};

typedef struct {
    uint32_t flags;
    uint8_t report[64];
    uint32_t buttons;  // Changed from uint16_t to support D-pad flags
    uint8_t lstick_x;
    uint8_t lstick_y;
    uint8_t rstick_x;
    uint8_t rstick_y;
    uint8_t battery;

    // FSM state for initialization
    switch_init_state init_state;
    uint8_t controller_type;
    uint8_t packet_num;  // Incremented for each subcommand

    // For change detection to reduce input spam
    uint32_t last_buttons;
    uint8_t last_lstick_x;
    uint8_t last_lstick_y;

    // Calibration data
    switch_cal_stick cal_x;
    switch_cal_stick cal_y;
    switch_cal_stick cal_rx;
    switch_cal_stick cal_ry;

    uint32_t get_buttons() {
        return buttons;
    }
} switch_state;

// report all of the switch controller states
extern switch_state switch_states[4];
uint32_t switch_map(int index, const uint32_t* buttons);

// minimal hid interface
int hid_init(const char* local_name);
int hid_update();
int hid_close();
int hid_get(uint8_t* dst, int dst_len); //

void gui_msg(const char* msg);                                  // temporarily display a msg

#ifdef __cplusplus
}
#endif

#endif /* hid_server_hpp */
