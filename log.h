#include <cstdint>

#define LOG_MSG_BOOT 0x1
#define LOG_MSG_PEAK 0x2
#define LOG_MSG_REGULAR 0x3
#define LOG_MSG_WATCHDOG 0x4
#define LOG_MSG_CONN_ERROR 0x5


const char* log_file_name = "flight_log";
struct log_event_t {
    uint64_t timestamp;
    uint8_t  message;
    int16_t  accel_x;
    int16_t  accel_y;
    int16_t  accel_z;
    uint8_t  checksum;
} __attribute__((packed));