/*
mcp3008hwspi: fast MCP3008 reader for Raspberry Pi
License: https://github.com/nagimov/mcp3008hwspi/blob/master/LICENSE
Readme: https://github.com/nagimov/mcp3008hwspi/blob/master/README.md
*/

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

long int ns_led;
long int ns_sensor;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t convar = PTHREAD_COND_INITIALIZER;
int shared_data = 0; // Shared data

void printUsage() {
    printf("mcp3008hwspi (version %s) \n"
           "Reads data from MCP3008 ADC through hardware SPI interface on Raspberry Pi.\n"
           "Online help, docs & bug reports: <https://github.com/nagimov/mcp3008hwspi>\n"
           "\n"
           "Usage: mcp3008hwspi [OPTION]... \n"
           "Mandatory arguments to long options are mandatory for short options too.\n"
           "  -b, --block B        read B blocks per every scan of all specified channels,\n"
           "                       1 <= BPR <= %i (default: %i) [integer];\n"
           "                       multiple channels are always read as a single block;\n"
           "  -r, --clockrate CR   SPI clock rate, Hz, %i <= CR <= %i\n"
           "                       (default: %i) [integer];\n"
           "                       MCP3008 must be powered from 5V for 3.6MHz clock rate;\n"
           "  -c, --channels CH    read specified channels CH, 0 <= CH <= 7 (default: %i);\n"
           "                       multiple channels can be specified, e.g. -c 0123;\n"
           "                       all channels are read as a single block, e.g. if ran as\n"
           "                       <mcp3008hwspi -c 0123 -b 2>\n"
           "                       8 blocks are transmitted per SPI read (4 channels x 2);\n"
           "  -s, --save FILE      save data to specified FILE (if not specified, data is\n"
           "                       printed to stdout);\n"
           "  -n, --samples N      set the number of samples per channel to be read to N\n"
           "                       (default: %i samples) [integer];\n"
           "  -f, --freq FREQ      set the sampling rate to FREQ, samples per second\n"
           "                       (default: %i Hz) [integer];\n"
           "                       if set to 0, ADC is sampled at maximum achievable rate,\n"
           "                       if set to > 0, --block is reset to 1;\n"
           "\n"
           "Data is streamed in comma separated format, e. g.:\n"
           "  sample ch0,  value ch0,  sample ch1,  value ch1\n"
           "           0,       1023,           1,        512\n"
           "           2,       1022,           3,        513\n"
           "         ...,        ...,         ...,        ...\n"
           "  samples are (hopefully) equally spaced in time;\n"
           "  channels are read sequentially with equal time delays between samples;\n"
           "  value chX shows raw 10-bit integer readback from channel X;\n"
           "  average sampling rate is written to both stdout and output file header.\n"
           "\n"
           "Exit status:\n"
           "  0  if OK\n"
           "  1  if error occurred while reading or wrong cmdline arguments.\n"
           "\n"
           "Example:\n"
           "  mcp3008hwspi  -r 3600000  -c 0123  -s out.csv  -f 0  -n 1000  -b 25\n"
           "                      ^         ^         ^        ^      ^        ^\n"
           "                      |         |         |        |      |        |\n"
           "  3.6 MHz SPI clock --+         |         |        |      |        |\n"
           "  read channels 0, 1, 2 and 3 --+         |        |      |        |\n"
           "  save data to output file 'out.csv' -----+        |      |        |\n"
           "  set sampling frequency to max achievable rate ---+      |        |\n"
           "  read 1000 samples per channel (1000 x 4 = 4000 total) --+        |\n"
           "  read channels in blocks of 25 (25 x 4 = 100 blocks per SPI read)-+\n"
           "",
           codeVersion, blocksMax, blocksDefault, clockRateMin, clockRateMax, clockRateDefault,
           channelDefault, samplesDefault, freqDefault);
}

void* fn(void* param) {
    char **argv = (char**) param;

    int argc = 0;
    while(argv[argc] != NULL) {
        printf("argv[%d] = %s\n", argc, argv[argc]);
        argc++;
    }
    printf("There are %d arguments\n", argc);
    
    int i, j;
    int ch_len = 0;
    int bSave = 0;
    char vSave[256] = "";
    int vSamples = samplesDefault;
    double vFreq = freqDefault;
    int vBlocks = blocksDefault;
    int vClockRate = clockRateDefault;
    int vChannel;
    long int ns;
    long int s;
    struct timespec spec;
    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-b") == 0) || (strcmp(argv[i], "--block") == 0)) {
            if (i + 1 <= argc - 1) {
                i++;
                vBlocks = atoi(argv[i]);
                if ((vBlocks < 1) || (vBlocks > blocksMax)) {
                    printf("Wrong blocks per read value specified!\n\n");
                    printUsage();
                   // return 1;
                }
            } else {
                printUsage();
               // return 1;
            }
        } else if ((strcmp(argv[i], "-r") == 0) || (strcmp(argv[i], "--clockrate") == 0)) {
            if (i + 1 <= argc - 1) {
                i++;
                vClockRate = atoi(argv[i]);
                if ((vClockRate < clockRateMin) || (vClockRate > clockRateMax)) {
                    printf("Wrong clock rate value specified!\n\n");
                    printUsage();
                    //return 1;
                }
            } else {
                printUsage();
               // return 1;
            }
        } else if ((strcmp(argv[i], "-c") == 0) || (strcmp(argv[i], "--channels") == 0)) {
            if (i + 1 <= argc - 1) {
                i++;
                ch_len = strlen(argv[i]);
                memset(selectedChannels, 0, sizeof(selectedChannels));
                for (j = 0; j < ch_len; j++) {
                    vChannel = argv[i][j] - '0';
                    if ((vChannel < 0) || (vChannel > 7)) {
                        printf("Wrong channel %d specified!\n\n", vChannel);
                        printUsage();
                        //return 1;
                    }
                    if (selectedChannels[vChannel]) {
                        printf("Channel %d listed more then once!\n", vChannel);
                        printUsage();
                    }
                    selectedChannels[vChannel] = 1;
                    channels[j] = vChannel;
                }
            } else {
                printUsage();
                //return 1;
            }
        } else if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--save") == 0)) {
            bSave = 1;
            if (i + 1 <= argc - 1) {
                i++;
                strcpy(vSave, argv[i]);
            } else {
                printUsage();
                //return 1;
            }
        } else if ((strcmp(argv[i], "-n") == 0) || (strcmp(argv[i], "--samples") == 0)) {
            if (i + 1 <= argc - 1) {
                i++;
                vSamples = atoi(argv[i]);
            } else {
                printUsage();
                //return 1;
            }
        } else if ((strcmp(argv[i], "-f") == 0) || (strcmp(argv[i], "--freq") == 0)) {
            if (i + 1 <= argc - 1) {
                i++;
                vFreq = atoi(argv[i]);
                vBlocks = 1;
                if (vFreq < 0) {
                    printf("Wrong sampling rate specified!\n\n");
                    printUsage();
                    //return 1;
                }
            } else {
                printUsage();
               // return 1;
            }
        } else {
            printUsage();
            //return 1;
        }
    }
    if (ch_len == 0) {
        ch_len = 1;
        channels[0] = channelDefault;
    }
    int microDelay = 0;
    if (vFreq != 0) {
        microDelay = 1000000 / vFreq;
    }
    int count = 0;
    int fd = 0;
    int val;
    struct timeval start, end;
    struct timespec ts;
    char buff[100];
    double diff;
    double rate;
    int *data;
    long int time;
    long int time2;
    long int prev_time;
    long int delay;
    long int delay2;
    data = malloc(ch_len * vSamples * sizeof(int));
    struct spi_ioc_transfer *tr = 0;
    unsigned char *tx = 0;
    unsigned char *rx = 0;
    tr = (struct spi_ioc_transfer *)malloc(ch_len * vBlocks * sizeof(struct spi_ioc_transfer));
    if (!tr) {
        perror("malloc");
        goto loop_done;
    }
    tx = (unsigned char *)malloc(ch_len * vBlocks * 4);
    if (!tx) {
        perror("malloc");
        goto loop_done;
    }
    rx = (unsigned char *)malloc(ch_len * vBlocks * 4);
    if (!rx) {
        perror("malloc");
        goto loop_done;
    }
    memset(tr, 0, ch_len * vBlocks * sizeof(struct spi_ioc_transfer));
    memset(tx, 0, ch_len * vBlocks);
    memset(rx, 0, ch_len * vBlocks);
    for (i = 0; i < vBlocks; i++) {
        for (j = 0; j < ch_len; j++) {
            tx[(i * ch_len + j) * 4] = 0x60 | (channels[j] << 2);
            tr[i * ch_len + j].tx_buf = (unsigned long)&tx[(i * ch_len + j) * 4];
            tr[i * ch_len + j].rx_buf = (unsigned long)&rx[(i * ch_len + j) * 4];
            tr[i * ch_len + j].len = 3;
            tr[i * ch_len + j].speed_hz = vClockRate;
            tr[i * ch_len + j].cs_change = 1;
        }
    }
    tr[ch_len * vBlocks - 1].cs_change = 0;
    fd = open(spidev_path, O_RDWR);
    if (fd < 0) {
        perror("open()");
        printf("%s\n", spidev_path);
        goto loop_done;
    }
    while (count < coldSamples) {
        if (ioctl(fd, SPI_IOC_MESSAGE(ch_len * vBlocks), tr) < 0) {
            perror("ioctl");
            goto loop_done;
        }
        count += ch_len * vBlocks;
    }
    count = 0;
    if (gettimeofday(&start, NULL) < 0) {
        perror("gettimeofday: start");
       // return 1;
    }
    int prev_val;
    int firstloop = 0; 
    int first_val;
    int dif_of_values;
    while (count < ch_len * vSamples) {
        if (ioctl(fd, SPI_IOC_MESSAGE(ch_len * vBlocks), tr) < 0) {
            perror("ioctl");
            goto loop_done;
        }
        for (i = 0, j = 0; i < ch_len * vBlocks; i++, j += 4) {
            timespec_get(&ts, TIME_UTC);
            strftime(buff, sizeof buff, "%D %T", gmtime(&ts.tv_sec));
            time2 = ts.tv_nsec;
            val = (rx[j + 1] << 2) + (rx[j + 2] >> 6);
            //printf("%d\n", val);
            timespec_get(&ts, TIME_UTC);
            strftime(buff, sizeof buff, "%D %T", gmtime(&ts.tv_sec));
            time = ts.tv_nsec;
            //delay = time - time2;
            //delay2 = time - prev_time;
            //printf("%ld\n", delay2);
            data[count + i] = val;
            if (firstloop == 0){
                first_val = val;
                }
                else{
                    dif_of_values = val - first_val;}
           // printf(" %d %d \n",i, data[count+i]);
           
            if(dif_of_values > 100 && firstloop == 1 ){
                ns_sensor = time;
                printf(" \n Current time: %s.%09ld UTC ", buff, time);
                //printf(" \n Current time: %s.%09ld UTC ", buff, time);
                //printf("in if %d\n", val);
                //sprintf("prev %d\n", data[count + (i-1)]);
                    if (fd)
                      close(fd);
                    if (rx)
                        free(rx);
                    if (tx)
                        free(tx);
                    if (tr)
                        free(tr);
                    pthread_exit(NULL);
               // printf("\n %d %ld\n",val, time);
                }
             prev_val = val;
             //prev_time = time;
             pthread_mutex_lock(&mtx);
             shared_data = 1;
             pthread_cond_signal(&convar);
             pthread_mutex_unlock(&mtx);
             firstloop = 1;
        
        }
        count += ch_len * vBlocks;
        if (microDelay > 0) {
            usleep(microDelay);
        }
    }
    if (count > 0) {
        if (gettimeofday(&end, NULL) < 0) {
            perror("gettimeofday: end");
        } else {
            if (end.tv_usec > start.tv_usec) {
                diff = (double)(end.tv_usec - start.tv_usec);
            } else {
                diff = (double)((1000000 + end.tv_usec) - start.tv_usec);
                end.tv_sec--;
            }
            diff /= 1000000.0;
            diff += (double)(end.tv_sec - start.tv_sec);
            if (diff > 0.0)
                rate = count / diff;
            else
                rate = 0.0;
        }
    }
 //   printf("%0.2lf seconds, %d samples, %0.2lf Hz total sample rate, %0.2lf Hz per-channel sample "
        //   "rate\n"
         //  "",
          // diff, count, rate, rate / ch_len);
    //clock_gettime(CLOCK_REALTIME, &spec);
   // ns = spec.tv_nsec;
   // printf("Current time: %ld nanoseconds since the Epoch\n", ns);
    if (bSave == 1) {
        printf("Writing to the output file...\n");
        FILE *f;
        f = fopen(vSave, "w");
        fprintf(f,
                "# %0.2lf seconds, %d samples, %0.2lf Hz total sample rate, %0.2lf Hz per-channel "
                "sample rate\n"
                "",
                diff, count, rate, rate / ch_len);
        fprintf(f, "sample ch%d, value ch%d", channels[0], channels[0]);
             timespec_get(&ts, TIME_UTC);
             strftime(buff, sizeof buff, "%D %T", gmtime(&ts.tv_sec));
             fprintf(f, "\n Current time: %s.%09ld UTC\n", buff, ts.tv_nsec);
        if (ch_len > 1) {
            for (i = 1; i < ch_len; i++) {
                fprintf(f, ", sample ch%d, value ch%d", channels[i], channels[i]);
                timespec_get(&ts, TIME_UTC);
                strftime(buff, sizeof buff, "%D %T", gmtime(&ts.tv_sec));
                fprintf(f, "Current time: %s.%09ld UTC\n", buff, ts.tv_nsec);
            }
        }
        fprintf(f, "\n");
        for (i = 0; i < vSamples; i++) {
            fprintf(f, "%d, %d", i * ch_len, data[i * ch_len]);
             clock_gettime(CLOCK_REALTIME, &spec);
             timespec_get(&ts, TIME_UTC);
             strftime(buff, sizeof buff, "%D %T", gmtime(&ts.tv_sec));
             fprintf(f, "\n Current time: %s.%09ld UTC\n", buff, ts.tv_nsec);
             fflush(f);
            if (ch_len > 1) {
                for (j = 1; j < ch_len; j++) {
                    fprintf(f, ", %d, %d", i * ch_len + j, data[i * ch_len + j]);
                   
                }
            }
            fprintf(f, "\n");
        }
        fclose(f);
    } else {
        printf("sample ch%d, value ch%d", channels[0], channels[0]);
        if (ch_len > 1) {
            for (i = 1; i < ch_len; i++) {
                //printf(", sample ch%d, value ch%d", channels[i], channels[i]);
            }
        }
        printf("\n");
        for (i = 0; i < vSamples; i++) {
           // printf("%d, %d", i * ch_len, data[i * ch_len]);
             timespec_get(&ts, TIME_UTC);
             strftime(buff, sizeof buff, "%D %T", gmtime(&ts.tv_sec));
             //printf("\n Current time: %s.%09ld UTC\n", buff, ts.tv_nsec);
            if (ch_len > 1) {
                for (j = 1; j < ch_len; j++) {
                    //printf(", %d, %d", i * ch_len + j, data[i * ch_len + j]);
                }
            }
            //printf("\n");
        }
    }
loop_done:
    if (fd)
        close(fd);
    if (rx)
        free(rx);
    if (tx)
        free(tx);
    if (tr)
        free(tr);
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
    while(i < 10){
    pthread_t thread_id, led; 
    FILE *pFile;
    printf("Before Thread\n"); 
    pthread_create(&thread_id, NULL, fn, argv); 
    pthread_create(&led, NULL, led_thread, NULL); 
    pthread_join(thread_id, NULL); 
    pthread_join(led, NULL); 
    printf("After Thread\n"); 
   // printf("%ld\n", ns_led);
    //printf("%ld\n", ns_sensor);
    long int delay;
    delay = ns_sensor - ns_led;
    printf("%ld\n", delay);
    pFile=fopen("4_1_camera_TIAnew.txt", "a");
    if(pFile==NULL) {
    perror("Error opening file.");
    }
    else {
       // fprintf(pFile, "\n%ld\n", ns_led);
        //fprintf(pFile, "%ld\n", ns_sensor);
        fprintf(pFile, "%ld\n", delay);
        //printf("%ld\n", delay);
    }
   fclose(pFile);
    //return 0;
    i++;
   ns_led = 0;
   ns_sensor = 0;
   shared_data = 0;
 }
}

