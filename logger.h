#ifndef __LOGGER_H_
#define __LOGGER_H_

#include <stddef.h>

#define MAX_FILENAME_LEN 255
#define DUMP_SIGNAL      SIGUSR1
#define DUMP_EXIT_SIGNAL SIGUSR2

enum log_option_t
{
	LOG_OFF    = 0,
	LOG_ON     = 1,
	LOG_MIN    = 2, // NO ADDITIONAL DATA
	LOG_STD    = 3, // TIME
	LOG_MAX    = 4  // TIME, FILENAME, FUNCTION NAME, LINE NUM
};

// API FUNCTIONS
int logger_start(const char *log_filename, void *dump_buffer, size_t buffer_size);
int logger_dump();

int logger_log_debug(const char *filename, const char *function, const int line, const char *format, ...);
#define logger_log(...) logger_log_debug(__FILE__,  __FUNCTION__,  __LINE__, __VA_ARGS__)
void logger_set_level(enum log_option_t);
void logger_stop();

#endif