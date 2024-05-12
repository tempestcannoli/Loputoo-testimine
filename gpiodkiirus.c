/*
mcp3008hwspi: fast MCP3008 reader for Raspberry Pi
License: https://github.com/nagimov/mcp3008hwspi/blob/master/LICENSE
Readme: https://github.com/nagimov/mcp3008hwspi/blob/master/README.md
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <gpiod.h>






int main (int argc, char *argv[]) {
    struct gpiod_chip* chip;
    struct gpiod_line_bulk lines;
    struct gpiod_line_request_config config;
    unsigned int offsets[1];
    int values[1];
    int err;
    struct timespec ts;
    chip = gpiod_chip_open("/dev/gpiochip4");
    if (!chip) {
        perror("gpiod_chip_open");
        goto cleanup;
    }

    offsets[0] = 16;
    values[0] = 0;

    err = gpiod_line_get_value_bulk(&lines, values);
    long int time_nanosec;
    long int time_nanosec2;
    timespec_get(&ts, TIME_UTC);
    time_nanosec = ts.tv_nsec; 
    if (err) {
        perror("gpiod_line_get_value_bulk");
        goto cleanup;
    }
    err = gpiod_line_get_value_bulk(&lines, values);
    timespec_get(&ts, TIME_UTC);
    time_nanosec2 = ts.tv_nsec; 
    if (err) {
        perror("gpiod_line_get_value_bulk");
        goto cleanup;
    }
    long int delay;
    delay = time_nanosec2 - time_nanosec;
    printf("delay %ld UTC\n", delay);
 
cleanup:
    gpiod_line_release_bulk(&lines);
    gpiod_chip_close(chip);
 
}

