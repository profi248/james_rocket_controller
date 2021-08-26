#include <cstdio>
#include "hardware/watchdog.h"

#include "servo.h"
#include "ADXL345_RP2040/ADXL345.h"
#include "lfs-pico.h"
#include "log.h"

#define ENABLE_FILESYSTEM 1

const uint microseconds_per_second = 1000000;
const double accelerometer_coefficient = ADXL345_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;

i2c_inst_t* accel_i2c_bus = i2c0;

const uint accel_sda_pin = 20;
const uint accel_scl_pin = 21;
const uint accel_interrupt_pin = 19;
const uint servo_pin = 2;
const uint piezo_pin = 7;
const uint button_pin = 6;
const uint led_pin = 25;

const uint start_servo_angle = 90;
const uint end_servo_angle = 180;
const uint freefall_treshold_mg = 600;
const uint freefall_time_ms = 200;

ADXL345 accelerometer;
volatile alarm_id_t piezo_alarm_id;
volatile bool       piezo_enabled = false;
volatile bool       piezo_on = false;
volatile bool       led_on = false;
volatile bool       wait_for_lock = true;
volatile bool       parachute_ejected = false;
volatile int64_t    parachute_ejected_timestamp = 0;

#if ENABLE_FILESYSTEM
lfs_t lfs;
lfs_file_t log_file;
#endif

int64_t piezo_callback(alarm_id_t id, void* user_data) {
    printf("callback fired, pin: %d\n", piezo_pin);
    uint delay_ms;

    if (piezo_on) {
        gpio_put(piezo_pin, false);
        piezo_on = false;
        delay_ms = 300;
    } else {
        gpio_put(piezo_pin, true);
        piezo_on = true;
        delay_ms = 1000;
    }

    piezo_alarm_id = add_alarm_in_ms(delay_ms, piezo_callback, nullptr, true);
    return 0;
}

void piezo_start() {
    gpio_put(piezo_pin, true);
    piezo_alarm_id = add_alarm_in_ms(1500, piezo_callback, nullptr, true);
    piezo_on = true;
}

void piezo_stop() {
    cancel_alarm(piezo_alarm_id);
    gpio_put(piezo_pin, false);
    piezo_on = false;
}

void set_checksum_byte(log_event_t* evt) {
    uint8_t* event = (uint8_t*) evt;
    uint8_t parity = event[0];

    // xor all other bytes except the checksum byte
    for (uint i = 1; i < sizeof(log_event_t) - 1; i++)
        parity ^= event[i];
    
    evt->checksum = parity;
}

void interrupt_handler(uint gpio, uint32_t events) {
    printf("interrupt! GPIO: %d events: 0x%x\n", gpio, events);
    if (gpio == accel_interrupt_pin) {
        uint8_t sources = accelerometer.getInterruptSources();
        printf("interrupt values: 0x%x\n", sources);
        if (accelerometer.isInterruptSource(ADXL345_INT_FREE_FALL, sources)) {
            set_degrees(servo_pin, end_servo_angle);

            if (!parachute_ejected) {
                piezo_start();
                parachute_ejected_timestamp = time_us_64();
            }

            #if ENABLE_FILESYSTEM
            if (!parachute_ejected) {
                log_event_t evt = { time_us_64(), LOG_MSG_PEAK, accelerometer.getX(), accelerometer.getY(), accelerometer.getZ(), 0 };
                set_checksum_byte(&evt);
                int ret = lfs_file_write(&lfs, &log_file, &evt, sizeof(evt));
                printf("wrote to file, returned: %d\n", ret);
                lfs_file_sync(&lfs, &log_file);
            }
            #endif

            parachute_ejected = true;
        }
    } else if (gpio == button_pin) {
        // pressing button locks the parachute if the microcontroller is after boot, or reboots if the parachute is ejected
        printf("button pressed!\n");
        if (wait_for_lock) {
            wait_for_lock = false;
        } else {
            // 20 seconds timer failsafe to try and prevent accidental restart
            if (parachute_ejected && parachute_ejected_timestamp + (20 * microseconds_per_second) < time_us_64()) {
                printf("restarting...\n");
                watchdog_enable(100, false);
                // burn cycles until watchdog kicks in
                while (true) { };
            }
        }
    }
}

void fast_blink_led_blocking(int count) {
    for (uint i = 0; i < count; i++) {
        gpio_put(led_pin, true);
        sleep_ms(100);
        gpio_put(led_pin, false);
        sleep_ms(50);
    }
}

int main() {
    stdio_init_all();
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    uint blink_cnt = watchdog_caused_reboot() ? 2 : 8;
    fast_blink_led_blocking(blink_cnt);

    // for USB console debugging
    if (!watchdog_caused_reboot())
        sleep_ms(2000);
    
    printf("hi :)\n");

    // initialize GPIO pins
    gpio_init(piezo_pin);
    gpio_set_dir(piezo_pin, GPIO_OUT);
    gpio_init(button_pin);
    gpio_set_dir(button_pin, GPIO_IN);
    gpio_init(accel_interrupt_pin);
    gpio_set_dir(accel_interrupt_pin, GPIO_IN);

    gpio_set_irq_enabled_with_callback(button_pin, GPIO_IRQ_EDGE_RISE, true, &interrupt_handler);

    set_servo(servo_pin);

    // set up accelerometer free fall interrupt
    accelerometer = ADXL345();
    accelerometer.begin(ADXL345_DEFAULT_ADDRESS, accel_i2c_bus, accel_sda_pin, accel_scl_pin);
    accelerometer.setRange(ADXL345_RANGE_16_G);
    gpio_set_irq_enabled_with_callback(accel_interrupt_pin, GPIO_IRQ_EDGE_RISE, true, &interrupt_handler);
    accelerometer.setFreefallInterruptTime(freefall_time_ms);
    accelerometer.setFreefallInterruptTreshold(freefall_treshold_mg);
    accelerometer.setInterrupt(ADXL345_INT_FREE_FALL, true);

    uint8_t status = watchdog_caused_reboot() ? LOG_MSG_WATCHDOG : LOG_MSG_BOOT;
    if (status == LOG_MSG_WATCHDOG) 
        printf("rebooted by watchdog!\n");

    // mount filesystem and log a boot (if enabled)
    #if ENABLE_FILESYSTEM
    printf("filesystem support enabled\n");
    int err = lfs_mount(&lfs, &cfg);

    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err) {
        printf("filesystem mount failed, reformatting...\n");
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }

    lfs_file_open(&lfs, &log_file, log_file_name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);

    log_event_t evt = { time_us_64(), status, 0, 0, 0, 0 };
    set_checksum_byte(&evt);
    int ret = lfs_file_write(&lfs, &log_file, &evt, sizeof(evt));
    printf("wrote to file, returned: %d\n", ret);
    lfs_file_sync(&lfs, &log_file);
    size_t size = lfs_file_size(&lfs, &log_file);
    printf("log file size: %d\n", size);
    #endif

    watchdog_enable(1500, 1);

    // wait for button press to start the program
    printf("waiting for button press (pin %d)...\n", button_pin);
    while (true) {
        if (!wait_for_lock)
            break;
        sleep_ms(1000);
        gpio_put(led_pin, !led_on);
        led_on = !led_on;
        watchdog_update();
    }

    // set motor to initial position
    set_degrees(servo_pin, start_servo_angle);

    printf("starting main loop...\n");
    while (true) {
        // blink the LED to indicate that the program is alive and periodically log the accelerometer data
        gpio_put(led_pin, !led_on);
        led_on = !led_on;
        uint8_t interrupt_enable_register = accelerometer.readRegister(ADXL345_REG_INT_ENABLE);

        // if value of the interrupt enable register is not what we expect, log an error
        if (interrupt_enable_register ^ ADXL345_INT_FREE_FALL) {
            printf("accelerometer connection error!\n");
            fast_blink_led_blocking(4);
            #if ENABLE_FILESYSTEM
            log_event_t evt = { time_us_64(), LOG_MSG_CONN_ERROR, accelerometer.getX(), accelerometer.getY(), accelerometer.getZ(), 0 };
            set_checksum_byte(&evt);
            int ret = lfs_file_write(&lfs, &log_file, &evt, sizeof(evt));
            printf("wrote to file, returned: %d\n", ret);
            lfs_file_sync(&lfs, &log_file);
            #endif
        } else {
            // periodically log accelerometer data
            #if ENABLE_FILESYSTEM
            log_event_t evt = { time_us_64(), LOG_MSG_REGULAR, accelerometer.getX(), accelerometer.getY(), accelerometer.getZ(), 0 };
            set_checksum_byte(&evt);
            int ret = lfs_file_write(&lfs, &log_file, &evt, sizeof(evt));
            printf("wrote to file, returned: %d\n", ret);
            lfs_file_sync(&lfs, &log_file);
            #endif
        }

        watchdog_update();
        sleep_ms(300);
    }
}
