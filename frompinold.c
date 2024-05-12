
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <gpiod.h>
#include <error.h>
#include <pthread.h>

#define MAX_ADC_CH 8
int selectedChannels[MAX_ADC_CH];
int channels[MAX_ADC_CH];
char spidev_path[] = "/dev/spidev0.0";
const char codeVersion[5] = "0.0.1";
const int blocksDefault = 1;
const int blocksMax = 511;
const int channelDefault = 0;
const int samplesDefault = 1000;
const int freqDefault = 0;
const int clockRateDefault = 3600000;
const int clockRateMin = 1000000;
const int clockRateMax = 3600000;
const int coldSamples = 10000;



pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t convar = PTHREAD_COND_INITIALIZER;
int shared_data = 0; // Shared data

long int ns_led = 0;
long int ns_sensor = 0;
long int sec_led = 0;
long int sec_sensor = 0;



void* fn(void* param) {
    struct gpiod_chip* chip;
    struct gpiod_line_bulk lines;
    struct gpiod_line_request_config config;
    unsigned int offsets[1];
    char buff[100];
    char buff2[100];
    int values[1];
    int err;
    int prev_val = 0;
    struct timespec ts;
    chip = gpiod_chip_open("/dev/gpiochip4");
    if (!chip) {
        perror("gpiod_chip_open");
        goto cleanup;
    }

    offsets[0] = 16;
    values[0] = 0;

    err = gpiod_chip_get_lines(chip, offsets, 1, &lines);
    if (err) {
        perror("gpiod_chip_get_lines");
        goto cleanup;
    }

    memset(&config, 0, sizeof(config));
    config.consumer = "read_input";
    config.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT; // Set as input
    config.flags = 0;
    err = gpiod_line_request_bulk(&lines, &config, values);
    if (err) {
        perror("gpiod_line_request_bulk");
        goto cleanup;
    }
    int loop = 0;
    int firstloop = 0;
    // Read the input value
    int first_loop = 0;
    while (loop == 0){
    err = gpiod_line_get_value_bulk(&lines, values);
    printf("%d\n", values[0]);
    long int time_nanosec;
    timespec_get(&ts, TIME_UTC);
    strftime(buff, sizeof buff2, "%D %T", gmtime(&ts.tv_sec));
    time_nanosec = ts.tv_nsec;
    if (err) {
        perror("gpiod_line_get_value_bulk");
        goto cleanup;
    }
    if(values[0] == 0 && firstloop == 1){
    printf("Current time sensor: %s.%09ld UTC\n", buff, time_nanosec);
    ns_sensor = time_nanosec;
    sec_sensor =  ts.tv_sec;
    loop = 1;
        }
        if (firstloop == 0){
        pthread_mutex_lock(&mtx);
        shared_data = 1;
        pthread_cond_signal(&convar);
        pthread_mutex_unlock(&mtx);
        firstloop = 1;
        }
   }

cleanup:
    gpiod_line_release_bulk(&lines);
    gpiod_chip_close(chip);
}



void *led_thread(void *vargp)
{
  pthread_mutex_lock(&mtx);
  while (shared_data != 1) {
        pthread_cond_wait(&convar, &mtx);
    }
  pthread_mutex_unlock(&mtx);
  struct gpiod_chip* chip;
  struct gpiod_line_bulk lines;
  struct gpiod_line_request_config config;
  unsigned int offsets[1];
  char buff[100];

  int values[1];
  int err;
  int i = 0;
  chip = gpiod_chip_open("/dev/gpiochip4");
  if(!chip)
  {
    perror("gpiod_chip_open");
    goto cleanup;
  }

  // set pin 21 to 1 (logic high)
  offsets[0] = 14;
  values[0] = 0;

  err = gpiod_chip_get_lines(chip, offsets, 1, &lines);
  if(err)
  {
    perror("gpiod_chip_get_lines");
    goto cleanup;
  }

  memset(&config, 0, sizeof(config));
  config.consumer = "blink";
  config.request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;
  config.flags = 0;

  // get the bulk lines setting default value to 0
  err = gpiod_line_request_bulk(&lines, &config, values);
  if(err)
  {
    perror("gpiod_line_request_bulk");
    goto cleanup;
  }
  // output value 1 to turn on the led
  struct timespec ts;
  long int led_time;
  //while (i < 3) {
  values[0] = 1;
  err = gpiod_line_set_value_bulk(&lines, values);
  timespec_get(&ts, TIME_UTC);
  strftime(buff, sizeof buff, "%D %T", gmtime(&ts.tv_sec));
  led_time = ts.tv_nsec;
  printf("Current time led: %s.%09ld UTC\n", buff, led_time);
  ns_led = led_time;
  sec_led = ts.tv_sec;
  if(err)
  {
    perror("gpiod_line_set_value_bulk");
    goto cleanup;
  }
  sleep(1);

  // output value 0 to turn off the led
  values[0] = 0;
  err = gpiod_line_set_value_bulk(&lines, values);
  if(err)
  {
    perror("gpiod_line_set_value_bulk");
    goto cleanup;
  }
  sleep(1);
  //i++;
//}
cleanup:
  gpiod_line_release_bulk(&lines);
  gpiod_chip_close(chip);

}


int main (int argc, char *argv[]) {
    int i = 0;
    while (i < 2){
    pthread_t thread_id, led;
    FILE *pFile;;
    pthread_create(&thread_id, NULL, fn, argv);
    pthread_create(&led, NULL, led_thread, NULL);
    pthread_join(thread_id, NULL);
    pthread_join(led, NULL);
    long int delay;
    long int sec_delay;
    sec_delay = sec_sensor - sec_led;
    delay = ns_sensor - ns_led;
    pFile=fopen("olddirect.txt", "a");
    if(pFile==NULL) {
    perror("Error opening file.");
    }
    else {
    if (sec_delay == 0){
    printf("%ld", delay);
    fprintf(pFile, "%ld\n", delay);
    }else{
      if(delay < 0){
        sec_delay = sec_delay - 1;
        delay += 1000000000L;
        }
        printf("%ld seconds and %ld nanoseconds",sec_delay, delay);
        fprintf(pFile, "%ld seconds and %ld nanoseconds",sec_delay, delay);


      }
    }
   fclose(pFile);
   i++;
   ns_led = 0;
   ns_sensor = 0;
   shared_data = 0;
   return 0;
}
}

