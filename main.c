#define _XOPEN_SOURCE
#include "cron.h"
#include <stdio.h>
#include <time.h>
#include <string.h>


int plan_task(int argc, char const *argv[]);
int cancel_task(int argc, char const *argv[]);
void display_help();


int main(int argc, char const *argv[])
{
	int ret = 0;

	// PARSE COMMAND LINE ARGUMENTS
	if (argc < 2)
	{	
		printf("Not enough arguments! Use -h for help.\n");
		return -1;
	}
	if (strlen(argv[1]) != 2)
	{
		printf("Improper first argument! Use -h for help.\n");
		return -2;
	}
	char opt = argv[1][1];
	switch (opt)
	{
		case 'p':
		{
			ret = plan_task(argc, argv);
			if (ret == 0)
			{
				printf("Task has been planned.\n");
			}
			else if (ret == 1) {
				printf("Cron daemon is not running.\n");
			}
			else
			{
				printf("Improper arguments for this option! Use -h for help.\n");
			}
			break;
		}
		case 'g':
		{
			ret = cron_get_task_list();
			if (ret == 1)
			{
				printf("Cron daemon is not running.\n");
			}
			break;
		}
		case 's':
		{
			ret = cron_start();
			if (ret == 0)
			{
				printf("Cron daemon has been started successfully.\n");
			}
			else if (ret == 1)
			{
				printf("Cron daemon is already running.\n");
			}
			else {
				printf("Cron daemon couldn't be started. Err: %d\n", ret);
			}
			break;
		}
		case 'q':
		{
			struct message_t msg = { 0 };
			msg.pid = getpid();
			msg.flag = CRON_QUIT;
			ret = cron_send(msg);
			if (ret == 0)
			{
				printf("Cron daemon has been stopped successfully.\n");
			}
			else if (ret == 1)
			{
				printf("Cron daemon is not running.\n");
			}
			else {
				printf("Cron daemon couldn't be stoppped. Err: %d\n", ret);
			}
			break;
		}
		case 'c':
		{
			ret = cancel_task(argc, argv);
			if (ret == 0)
			{
				printf("Task has been cancelled.\n");
			}
			else if (ret == 1)
			{
				printf("Cron daemon is not running.\n");
			}
			else {
				printf("Improper arguments for this option! Use -h for help.\n");
			}
			break;
		}
		case 'h':
		{
			display_help();
			break;
		}
	}
	return 0;
}

int plan_task(int argc, char const *argv[])
{
	struct message_t msg = { 0 };
	msg.pid = getpid();
	// STANDARD PLAN
	if (argc < 5)
	{
		return -1;
	}
	int program_idx = 4;
	struct tm time_struct;
	time_t now, arg_time;
	char time_string[MAX_STRING_LEN] = { 0 };
	strcpy(time_string, argv[2]);
	strcat(time_string, " ");
	strcat(time_string, argv[3]);
	double seconds = 0;
	// PARSE ABSOLUTE TIME
	if (strptime(time_string, "%d/%m/%Y %T", &time_struct) != NULL)
	{
		if (time(&now) == (time_t) -1)
		{
			return -2;
		}
		arg_time = mktime(&time_struct);
		seconds = difftime(arg_time, now);
	}
	// PARSE RELATIVE TIME
	else if (sscanf(argv[2], "%lf", &seconds) == 1)
	{
		program_idx = 3;
	}
	else
	{
		return -3;
	}
	// CHECK TIME
	if (seconds <= 0)
	{
		return -4;
	}
	msg.seconds = seconds;
	// PARSE PROGRAM
	if (strlen(argv[program_idx]) == 2 && argv[program_idx][1] == 'r')
	{
		msg.flag = CRON_TASK_PERIODIC;
		program_idx += 1;
	}
	else
	{
		msg.flag = CRON_TASK_STANDARD;
	}
	if (strlen(argv[program_idx]) > MAX_STRING_LEN - 1)
	{
		return -5;
	}
	char query_progr[MAX_STRING_LEN + 23] = { 0 };
	strcpy(query_progr, "which ");
	strcat(query_progr, argv[program_idx]);
	strcat(query_progr, " > /dev/null 2>&1");
	// COMMAND NOT IN $PATH
	if (system(query_progr))
	{
		// COMMAND NOT IN FILE SYSTEM
		if (access(argv[program_idx], F_OK) != 0)
		{
			return -6;
		}
	}
	strcpy(msg.program, argv[program_idx]);

	// PARSE ARGUMENTS
	for (int i = program_idx + 1; i < argc; ++i)
	{
		if (strlen(msg.args) + strlen(argv[i]) + 1 > MAX_STRING_LEN - 1)
		{
			return -7;
		}
		strcat(msg.args, argv[i]);
		strcat(msg.args, " ");
	}
	return cron_send(msg);
}

int cancel_task(int argc, char const *argv[])
{
	struct message_t msg = { 0 };
	msg.pid = getpid();
	if (argc < 2)
	{
		return -1;
	}
	int task_id = -1;
	int ret = sscanf(argv[2], "%d", &task_id);
	if (ret != 1)
	{
		return -2;
	}
	if (task_id < 0)
	{
		return -3;
	}
	msg.flag = CRON_TASK_CANCEL;
	msg.seconds = task_id;
	return cron_send(msg);
}
void display_help()
{
	printf("\nNAME\n");
	printf("\tcron - real time task scheduler\n");
	printf("\nOPTIONS\n");

	printf("\n\t-s\n\n");
	printf("\t\tStart cron daemon. After this command, cron runs in the\n");
	printf("\t\tbackground.\n");

	printf("\n\t-q\n\n");
	printf("\t\tSchedules cron daemon to quit and cancel all it's tasks.\n");

	printf("\n\t-h\n\n");
	printf("\t\tDisplays help about cron.\n");

	printf("\n\t-p <datetime/seconds> <-r> <program path> <arguments>\n\n");
	printf("\t\tSchedules cron a task in given time which can be absolute in\n");
	printf("\t\t<YYYY/MM/DD HH:MM:SS> format or it can also be relative and\n");
	printf("\t\tgiven in seconds. Optional parameter defines a task as\n");
	printf("\t\tperiodic. It means that the scheduled progam is going to\n");
	printf("\t\trepeat itself in given interval. It only works when time\n");
	printf("\t\tis given in seconds.\n");

	printf("\n\t-g\n\n");
	printf("\t\tDisplays all currently scheduled tasks with their IDs, names and time.\n");
	
	printf("\n\t-c <task ID>\n\n");
	printf("\t\tCancels scheduled task with given task ID.\n");
	printf("\n");
}