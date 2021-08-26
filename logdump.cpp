#include <stdio.h>

#include "log.h"
#include "lfs-pico.h"

const uint led_pin = 25;

lfs_t lfs;
lfs_file_t log_file;

int main() {
    stdio_init_all();
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    sleep_ms(5000);
    gpio_put(led_pin, true);

    // mount the filesystem
    int err = lfs_mount(&lfs, &cfg);

    if (err) {
        printf("filesystem mount failed! return: %d\n", err);
    }

    lfs_file_open(&lfs, &log_file, log_file_name, LFS_O_RDONLY | LFS_O_CREAT);
    size_t size = lfs_file_size(&lfs, &log_file);
    size_t total = 0;
    printf("start %lu\n", size);
    uint8_t* buf = (uint8_t*) malloc(sizeof(log_event_t));

    while (total < size) {
        memset(buf, 0, sizeof(log_event_t));
        int read = lfs_file_read(&lfs, &log_file, buf, sizeof(log_event_t));

        if (read > 0)
            total += read;
        else
            printf("read error: %d\n", read);
        
        for (size_t i = 0; i < read; i++) printf("%02X ", (buf)[i]); 
        printf("\n");
    }

    printf("end\n");
    
    for (uint i = 0; i < 20; i++) {
        gpio_put(led_pin, true);
        sleep_ms(100);
        gpio_put(led_pin, false);
        sleep_ms(50);
    }

    free(buf);
    lfs_file_close(&lfs, &log_file);
}
