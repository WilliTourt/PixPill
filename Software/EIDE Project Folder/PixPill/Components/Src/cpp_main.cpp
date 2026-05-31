#include "cpp_main.h"

#include "bma530.h"
#include "is31fl3736.h"
#include "ElegantDebug.h"

#include "sandsim.h"

BMA530 accel(&hi2c1);
IS31FL3736 is31(&hi2c1);
ElegantDebug debug(&huart2, true, true, true);

SandSim sand(accel, is31);

int16_t abs(int16_t value) {
    return (value < 0) ? -value : value;
}

// uint16_t rand(int32_t seed) {
//     seed = (seed * 114 + 514) % 1000000;
//     seed = (0xDEADBEEF / seed) * 7 + 12345;
//     return (uint16_t)(seed % 65536);
// }

void cpp_main() {

    debug.info("Initiating %s%sBMA530%s accelerometer...\r\n", BOLD, COLOR_MAGENTA, CLR);
    if (accel.begin(BMA530::ODR::_100HZ, BMA530::Range::_2G, BMA530::Power::LPM)) {
        debug.success("BMA530 accelerometer initialized.\r\n");
    } else {
        debug.error("Failed to initialize BMA530 accelerometer.\r\n");
    }

    debug.info("Initiating %s%sIS31FL3736%s LED driver...\r\n", BOLD, COLOR_RED, CLR);
    if (is31.begin(0x39)) {
        debug.success("IS31FL3736 LED driver initialized.\r\n");
        is31.setPWMAll(0xFF);

        debug.info("Testing IS31FL3736 by setting all LEDs on. GCC = 0x20... \r\n");
        is31.ledOnAll(0x20);
        HAL_Delay(1000);
        is31.ledOffAll();
    } else {
        debug.error("Failed to initialize IS31FL3736 LED driver.\r\n");
    }

    debug.info("Initiating %s%sSAND SIMULATION%s ...\r\n", BOLD, COLOR_DARK_YELLOW, CLR);
    sand.init();
    debug.success("SAND SIMULATION initialized. Starting simulation in 1000ms...\r\n");
    HAL_Delay(1000);
    sand.start();

    while (1) {
        accel.update();
        uint16_t tilt = abs(accel.readAx()) + abs(accel.readAy());
        uint16_t delay_ms = 100000 / (tilt + 500);
        debug.info("Tilt: %d, Delay: %d ms\r\n", tilt, delay_ms);
        if (delay_ms > 200) delay_ms = 200;
        if (delay_ms < 20)  delay_ms = 20;
        sand.calc();
        sand.draw();
        HAL_Delay(delay_ms);
    }
}


