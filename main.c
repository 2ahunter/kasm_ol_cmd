#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <time.h>
#include "UDP_client.h"

#define HOME 100
#define ZERO_SAMPLES 100


char ip[]="127.0.0.0";
char port[] = "2345";


/**
 * @brief Calculate the elapsed time between two timespec structures.
 * @param start The starting time.
 * @param end The ending time.
 * @return A timespec structure representing the elapsed time.
 */
struct timespec get_elapsed_time(struct timespec start, struct timespec end) {
    struct timespec elapsed;
    if ((end.tv_nsec - start.tv_nsec) < 0) { // account for nanosecond overflow
        elapsed.tv_sec = end.tv_sec - start.tv_sec - 1;
        elapsed.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else {
        elapsed.tv_sec = end.tv_sec - start.tv_sec;
        elapsed.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return elapsed;
}

/** 
 * @brief send command values to actuater.
 * @param cmd_val
 * @return status: 0 on success, -1 on failure
 * @note This function sends command values to the actuater via UDP.
 * It prepares the command data and sends it using the UDP_send function.
 */
int send_command(int16_t cmd_val) {
    union CMD_DATA cmd_data, buf_data;

    // Prepare the command data
    for(int i = 0; i < CMD_SIZE/2; i++) {
        cmd_data.values[i] = cmd_val; // Set command value
        buf_data.values[i] = htons(cmd_val); // Convert to network byte order
    }

    // Send the command buffer values
    size_t bytes_sent = UDP_send(buf_data);
    if (bytes_sent <= 0) {
        fprintf(stderr, "Failed to send command data: %s\n", strerror(errno));
        return -1; // Return error if sending fails
    }
    // printf("Sent %zu bytes\n", bytes_sent);
    return 0; // Return success

}


int main(int argc, char *argv[]) {

    // private variables 
    int opt = 0; // option for command line argument parsing
    char logfile[50] = "./testing/cmd_log.csv"; // default logfile name
    int log_fd = -1; // File descriptor for log file
    int num_samples = 500; // default number of samples to read
    int num_steps = 1; // Number of steps for command value increment
    int16_t cmd_inc = 1000; // Increment value for command
    struct timespec start_time; // t0
    struct timespec current_time; // t 
    struct timespec elapsed_time; // Timestamp for datalogging (t - t0)
    int period = 1000; // command interval in usec
    int start_cmd = 0;
    int end_cmd = 0;
    int16_t cmd_val = 0;
    int16_t max_cmd = 24000; // Maximum command value


    // Initialize the timer and logger 
    clock_gettime(CLOCK_MONOTONIC, &start_time); // Start time measurement
    openlog(NULL, LOG_PERROR, LOG_LOCAL6); // Open syslog for logging
    syslog(LOG_INFO, "Starting kasm_ol_cmd.\n");

    // Parse command line arguments for logfile, and number of samples
    while ((opt = getopt(argc, argv, "hi:b:e:n:l:s:")) != -1) {
        switch(opt) {
            case 'i':
                strcpy(ip, optarg); // Set IP address
                syslog(LOG_INFO, "IP address set to: %s\n", ip);
                break;
            case 'b':
                start_cmd = atoi(optarg);
                syslog(LOG_INFO, "Start command value set to %d", start_cmd);
                break;
            case 'e':
                end_cmd = atoi(optarg);
                syslog(LOG_INFO, "End command value set to %d", end_cmd);
                break;
            case 'l':
                strncpy(logfile, optarg, sizeof(logfile) - 1); // Set logfile name
                logfile[sizeof(logfile) - 1] = '\0'; // Ensure null termination
                syslog(LOG_INFO,"Datalog file set to: %s\n", logfile);
                break;
            case 'n':
                num_samples = atoi(optarg); // Set number of samples to read
                syslog(LOG_INFO, "Number of samples: %d\n", num_samples);
                if (num_samples <= 0 || num_samples > 1000) { // Validate number of samples
                    syslog(LOG_ERR, "Number of samples must be greater than 0 and less than 1000.\n");
                    return -1; // Exit if invalid number of samples
                }
                break;
            case 's':
                num_steps = atoi(optarg);
                if(num_steps <= 0){
                    syslog(LOG_ERR, "Number of steps must be greater than 0.\n");
                    return -1; // Exit if invalid number of steps
                }
                syslog(LOG_INFO, "Number of steps set to %d", num_steps);
                break;
            default:
                fprintf(stderr, "Usage: %s [-i ip] [-b start_cmd] [-e end_cmd] [-l logfile] [-n num_samples] [-s number of steps]\n", argv[0]);
                return -1; // Exit on invalid option
        }
    }

        // Initialize the UDP communication to the KASM PCB
    int fd=0;
    fd = UDP_init(ip, port);

    if(fd<0){
        syslog(LOG_ERR, "Failed to get socket descriptor");
        exit(EXIT_FAILURE);
    } else{
        syslog(LOG_INFO, "UDP client initialized");
    }

    // Set the actuator at the home position
    send_command(HOME);
    usleep(100000); // Sleep for 100ms to allow actuater to settle

    // Open the log file for writing only, create it if non-existent, and overwrite it if it exists
    log_fd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, 0666); // 
    if (log_fd == -1 ) {
        fprintf(stderr, "Failed to open log file %s: %s\n", logfile, strerror(errno));
        return -1; // Exit if log file cannot be opened
    }
    char log_header[] = "Channel,Timestamp,Value\n"; // Header for log file
    if (write(log_fd, log_header, sizeof(log_header) - 1) == -1) {
        fprintf(stderr, "Failed to write header to log file: %s\n", strerror(errno));
        close(log_fd);
        return -1; // Exit if writing header fails
    }

 
    char data_line[80]; 
    int line_length = 0;
    cmd_val = start_cmd; // Initialize command value to start command
    cmd_inc = (end_cmd - start_cmd) / num_steps; // Calculate command increment based on number of steps
    // start the command sequence
    for(int step = 0; step < num_steps; step++) {
        for(int i=0; i < num_samples; i++) {
            if (send_command(cmd_val+HOME) == -1) {
                syslog(LOG_ERR, "Failed to send command value %d: %s\n", cmd_val, strerror(errno));
                break;
            }

            clock_gettime(CLOCK_MONOTONIC, &current_time); // Get current time
            elapsed_time = get_elapsed_time(start_time, current_time);
            line_length = sprintf( data_line, "%ld.%09ld,%d\n", elapsed_time.tv_sec,elapsed_time.tv_nsec, cmd_val);
            printf("%s", data_line); // Print to console for debugging
            if (write(log_fd, data_line, strlen(data_line)) == -1) {
                fprintf(stderr, "Failed to write data to log file: %s\n", strerror(errno));
                close(log_fd);
                return -1; // Exit if writing data fails
            }
            usleep(period); // Sleep for the specified command interval
        }

        // Return actuator position to home for 100 sample periods before next step
        for(int i=0; i < ZERO_SAMPLES ; i++) {
            if (send_command(HOME) == -1) {
                syslog(LOG_ERR, "Failed to send command value %d: %s\n", HOME, strerror(errno));
                break;
            }

            clock_gettime(CLOCK_MONOTONIC, &current_time); // Get current time
            elapsed_time = get_elapsed_time(start_time, current_time); 
            line_length = sprintf( data_line, "%ld.%09ld,%d\n", elapsed_time.tv_sec,elapsed_time.tv_nsec, HOME);
            printf("%s", data_line); // Print to console for debugging
            if (write(log_fd, data_line, strlen(data_line)) == -1) {
                fprintf(stderr, "Failed to write data to log file: %s\n", strerror(errno));
                close(log_fd);
                return -1; // Exit if writing data fails
            }
            usleep(period); // Sleep for the specified command interval
        }

        cmd_val += cmd_inc; // increment command value for next step
        if(abs(cmd_val) > max_cmd) {
            syslog(LOG_ERR, "Command value exceeded maximum limit of %d. Stopping data collection.", max_cmd);
            break; 
        }
    }

    close(log_fd); 
    syslog(LOG_INFO, "Data collection complete.\n");
    closelog();
    return 0;

}

