#include "logger.h"

#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <stdarg.h>

// CONTROL STRUCT FOR LOGGER
struct logger_t
{
	enum log_option_t level;
	enum log_option_t state;
	char log_filename[MAX_FILENAME_LEN];
} g_logger = { 0 };

// CONTROL STRUCT FOR DUMP
struct dump_t
{
	sem_t     dump_sem;
	sem_t     dump_exit_sem;
	pthread_t dump_thread;
	void*	  dump_buffer;
	size_t    dump_buffer_size;
} g_dump = { 0 };

void dump_handler(int signo, siginfo_t* info, void* extra)
{
	if (signo == DUMP_EXIT_SIGNAL)
	{
		sem_post(&g_dump.dump_exit_sem);
		sem_post(&g_dump.dump_sem);
	}
	else
	{
		sem_post(&g_dump.dump_sem);
	}
}

void dump_generate_filename(char dest[MAX_FILENAME_LEN])
{
	static int dump_num = 0;

	if (dest != NULL)
	{
		time_t rawtime;
		struct tm * timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		char *time = asctime(timeinfo);
		// REPLACE LAST ENTER IN TIME
		if (strlen(time) > 0)
		{
			time[strlen(time) - 1] = '\0';
		}
		char *ptr = time;
		// REPLACE SPECIAL CHARS
		while (*ptr)
		{
			if (*ptr == ' ')
			{
				*ptr = '_';
			}
			if (*ptr == ':')
			{
				*ptr = '-';
			}
			ptr++;
		}
		// FINALIZE FILENAME
		sprintf(dest, "dump_%d_%s.bin", dump_num, time);
		dump_num++;
	}
}

void* dump_thread(void* args)
{
	// POST THAT THREAD IS STARTED
	sem_post(&g_dump.dump_exit_sem);
	// MAIN LOOP
	while (1)
	{
		// WAIT FOR OPERATION
		sem_wait(&g_dump.dump_sem);
		// CHECK FOR EXIT CALL
		int sem_val = -1;
		sem_getvalue(&g_dump.dump_sem, &sem_val);
		if (sem_val == 0 && sem_trywait(&g_dump.dump_exit_sem) == 0)
		{
			break;
		}
		// GENERATE DUMP FILENAME
		char filename[MAX_FILENAME_LEN] = { 0 };
		dump_generate_filename(filename);
		// DUMP APPLICATION STATE
		FILE *pFile = fopen(filename, "wb");
		if (pFile != NULL)
		{
			fwrite(g_dump.dump_buffer, g_dump.dump_buffer_size, 1, pFile);
			fclose(pFile);
		}
	}
}

int logger_start(const char *log_filename, void *dump_buffer, size_t buffer_size)
{
	// CHECK IF LOGGER IS ALREADY RUNNING
	if (g_logger.state == LOG_ON)
	{
		return 1;
	}
	// ARGUMENTS
	if (log_filename == NULL || strlen(log_filename) < 1 || dump_buffer == NULL || buffer_size < 1)
	{
		return -1;
	}
	// FILL GLOBAL STRUCTS
	g_logger.level = LOG_STD;
	g_logger.state = LOG_ON;
	g_dump.dump_buffer = dump_buffer;
	g_dump.dump_buffer_size = buffer_size;
	if (strlen(log_filename) > MAX_FILENAME_LEN - 1)
	{
		return 1;
	}
	strcpy(g_logger.log_filename, log_filename);
	// DUMP SEMAPHORE
	if (sem_init(&g_dump.dump_sem, 0, 0) != 0)
	{
		return 2;
	}
	// DUMP EXIT SEMAPHORE
	if (sem_init(&g_dump.dump_exit_sem, 0, 0) != 0)
	{
		sem_close(&g_dump.dump_sem);
		return 2;
	}
	// REGISTER DUMP SIGNAL
	struct sigaction sa;
	sa.sa_sigaction = dump_handler;
	sa.sa_flags = 0;
	if (sigaction(DUMP_SIGNAL, &sa, NULL) != 0)
	{
		sem_close(&g_dump.dump_sem);
		sem_close(&g_dump.dump_exit_sem);
		return 3;
	}
	// REGISTER DUMP EXIT SIGNAL
	if (sigaction(DUMP_EXIT_SIGNAL, &sa, NULL) != 0)
	{
		sem_close(&g_dump.dump_sem);
		sem_close(&g_dump.dump_exit_sem);
		return 4;
	}
	// START DUMP THREAD
	if (pthread_create(&g_dump.dump_thread, NULL, dump_thread, NULL) != 0)
	{
		sem_close(&g_dump.dump_sem);
		sem_close(&g_dump.dump_exit_sem);
		return 5;
	}
	// WAIT TILL DUMP THREAD IS CREATED
	sem_wait(&g_dump.dump_exit_sem);
}

int logger_dump()
{
	// CHECK IF LOGGER IS RUNNING
	if (g_logger.state != LOG_ON)
	{
		return 1;
	}
	// QUEUE A SIGNAL TO DUMP THREAD
	union sigval value;
	sigqueue(getpid(), DUMP_SIGNAL, value);
	return 0;
}

int logger_log_debug(const char *filename, const char *function, const int line, const char *format, ...) {
	va_list arg;
	int done = 0;
	FILE* pFile = fopen(g_logger.log_filename, "a+");
	if (pFile != NULL)
	{
		// MIN LOG LEVEL CASE
		if (g_logger.level >= LOG_MIN)
		{
			va_start(arg, format);
			// STD LOG LEVEL CASE
			if (g_logger.level >= LOG_STD)
			{
				time_t rawtime;
				struct tm * timeinfo;
				time(&rawtime);
				timeinfo = localtime(&rawtime);
				char *time = asctime(timeinfo);
				if (strlen(time) > 0)
				{
					time[strlen(time) - 1] = '\0';
				}
				fprintf(pFile, "[%s", time);
				// MAX LOG LEVEL CASE
				if (g_logger.level >= LOG_MAX)
				{
					fprintf(pFile, ", %s: %s(): %d line", filename, function, line);
				}
				fprintf(pFile, "]: ");
			}
			done = vfprintf(pFile, format, arg);
			fprintf(pFile, "\n");
			va_end(arg);
			fclose(pFile);
		}
	}
	return done;
}

void logger_set_level(enum log_option_t lvl) {
	if (lvl >= LOG_MIN && lvl <= LOG_MAX)
	{
		g_logger.level = lvl;
	}
}

void logger_stop()
{
	// CHECK IF LOGGER IS RUNNING
	if (g_logger.state == LOG_ON)
	{
		// QUEUE A SIGNAL TO DUMP THREAD
		union sigval value;
		sigqueue(getpid(), DUMP_EXIT_SIGNAL, value);
		// WAIT FOR DUMP THREAD TO COMPLETE
		pthread_join(g_dump.dump_thread, NULL);
		// CLOSE SEMAPHORES
		sem_close(&g_dump.dump_sem);
		sem_close(&g_dump.dump_exit_sem);
		// FILL GLOBAL STUCTS
		memset(&g_logger, 0, sizeof(struct logger_t));
		memset(&g_dump, 0, sizeof(struct dump_t));
	}
}