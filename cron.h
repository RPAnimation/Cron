#ifndef _CRON_H_
#define _CRON_H_

#include <mqueue.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>


#define SERVER_QUEUE_NAME "/cron_server"
#define CLIENT_QUEUE_NAME "/cron"
#define MAX_STRING_LEN 255
#define MAX_QUEUE_SIZE 5
#define MAX_TASK_LIST 100

enum flag_t
{
	CRON_QUIT = 0,
	CRON_TASK_STANDARD = 1,
	CRON_TASK_PERIODIC = 2,
	CRON_TASK_CANCEL = 3,
	CRON_TASK_SHOW = 4,
	CRON_FAILURE = 5,
	CRON_SUCCESS = 6
};

struct message_t
{
	char program[MAX_STRING_LEN];
	char args[MAX_STRING_LEN];
	double seconds;
	enum flag_t flag;
	int pid;
};

struct task_t {
	timer_t timer;
	struct message_t msg;
};


int cron_fork();
int cron_status();
int cron_start();
int cron_send(struct message_t msg);
int cron_server_loop();
int cron_get_task_list();
#endif