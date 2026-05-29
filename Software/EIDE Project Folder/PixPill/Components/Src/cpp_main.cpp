#include "cpp_main.h"

#include "bma530.h"
#include "is31fl3736.h"
#include "ElegantDebug.h"

BMA530 accel(&hi2c1);
IS31FL3736 is31(&hi2c1);
ElegantDebug debug(&huart2, true, true, true);

void cpp_main() {

    debug.info("Initiating %s%sBMA530%s accelerometer...\r\n", BOLD, COLOR_MAGENTA, CLR);
    if (accel.begin(BMA530::ODR::_100HZ, BMA530::Range::_2G, BMA530::Power::LPM)) {
        debug.success("BMA530 accelerometer initialized.\r\n");
    } else {
        debug.error("Failed to initialize BMA530 accelerometer.\r\n");
    }

    debug.info("Initiating %s%sIS31FL3736%s LED driver...\r\n", BOLD, COLOR_RED, CLR);
    if (is31.begin()) {
        debug.success("IS31FL3736 LED driver initialized.\r\n");
        
        debug.info("Testing IS31FL3736 by setting all LEDs on. GCC = 0x20... \r\n");
        if (is31.ledOnAll(0x20)) {
            debug.info("LEDs set.\r\n");
        } else {
            debug.warning("Failed to set LEDs.\r\n");
        }
    } else {
        debug.error("Failed to initialize IS31FL3736 LED driver.\r\n");
    }

    debug.info("Entering main loop in 1000ms.\r\n");
    HAL_Delay(1000);

    while (1) {
        // if (accel.isDataReady()) {
        //     accel.update();
        //     // int16_t ax = accel.readAx();
        //     // int16_t ay = accel.readAy();
        //     int16_t az = accel.readAz();

        //     uint8_t gcc = (az - 8192) / 64;
        //     if (gcc > 0x60) gcc = 0x60;
        //     if (gcc < 0) gcc = 0;
        //     is31.setGCC(gcc);

        //     // debug.info("New accelerometer data: Ax = %d, Ay = %d, Az = %d\r\n", ax, ay, az);
        // }
        // for (uint8_t i = 0; i < 0x80; i++) {
        //     is31.setGCC(i);
        //     HAL_Delay(5);
        // }
        // for (uint8_t i = 0x80; i > 0; i--) {
        //     is31.setGCC(i);
        //     HAL_Delay(5);
        // }

        for (uint8_t cs = 1; cs <= 8; cs++) {
            for (uint8_t sw = 1; sw <= 12; sw++) {
                is31.ledOffAll();
                is31.ledOn(cs, sw);
                is31.setPWM(cs, sw, 255);
                HAL_Delay(160);
            }
        }
    }
}
