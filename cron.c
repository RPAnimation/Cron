#include "cron.h"
#include "logger.h"

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

int cron_fork()
{
	// FORK CRON DEAMON
	pid_t pid, sid;
	pid = fork();
	if (pid < 0)
	{
		return 1;
	}
	if (pid > 0)
	{
		// WAIT FOR CRON TO START
		while (cron_status() == 0) {
			usleep(100);
		}
		printf("Daemon pid: %d\n", pid);
		// CLIENT RETURN
		return 0;
	}
	umask(0);
	sid = setsid();
	if (sid < 0)
	{
		return 1;
	}
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	
	// SERVER LOOP
	return cron_server_loop();
}

int cron_status()
{
	mqd_t check = mq_open(SERVER_QUEUE_NAME, O_RDONLY);
	if (check != (mqd_t) -1)
	{
		mq_close(check);
		return 1;
	}
	return 0;
}

int cron_start()
{
	// CRON ALREADY RUNNING
	if (cron_status() == 1)
	{
		return 1;
	}
	// START DEAMON
	if (cron_fork() != 0)
	{
		return 2;
	}
	// DEAMON RUNNING
	return 0;
}

int cron_send(struct message_t msg)
{
	// INIT QUEUE NAMES
	char rx_name[MAX_STRING_LEN]; sprintf(rx_name, "%s_%d", CLIENT_QUEUE_NAME, msg.pid);
	// CONNECT TO SERVER QUEUE
	mqd_t tx = mq_open(SERVER_QUEUE_NAME, O_WRONLY);
	if (tx == (mqd_t) -1)
	{
		return 1;
	}
	// CREATE CLIENT QUEUE
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_QUEUE_SIZE;
	attr.mq_msgsize = sizeof(struct message_t);
	attr.mq_curmsgs = 0;
	mqd_t rx = mq_open(rx_name, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
	if (rx == (mqd_t) -1)
	{
		mq_close(tx);
		return 2;	
	}
	// SEND MESSAGE
	if (mq_send(tx, (char *)&msg, sizeof(struct message_t), 1) == (mqd_t) -1)
	{
		mq_close(tx);
		mq_close(rx);
		mq_unlink(rx_name);
		return 3;
	}
	// WAIT FOR RESPONSE
	if (mq_receive(rx, (char *)&msg, sizeof(struct message_t), NULL) == (mqd_t) -1) {
		mq_close(tx);
		mq_close(rx);
		mq_unlink(rx_name);
		return 4;
	}
	if (msg.flag == CRON_FAILURE)
	{
		return 5;
	}
	// CLOSE QUEUES
	mq_close(tx);
	mq_close(rx);
	mq_unlink(rx_name);
	// ALL OK
	return 0;
}
int cron_get_task_list()
{
	// INITIAL MESSAGE
	struct message_t msg;
	msg.flag = CRON_TASK_SHOW;
	msg.pid = getpid();
	// INIT QUEUE NAMES
	char rx_name[MAX_STRING_LEN]; sprintf(rx_name, "%s_%d", CLIENT_QUEUE_NAME, msg.pid);
	// CONNECT TO SERVER QUEUE
	mqd_t tx = mq_open(SERVER_QUEUE_NAME, O_WRONLY);
	if (tx == (mqd_t) -1)
	{
		return 1;
	}
	// CREATE CLIENT QUEUE
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_QUEUE_SIZE;
	attr.mq_msgsize = sizeof(struct message_t);
	attr.mq_curmsgs = 0;
	mqd_t rx = mq_open(rx_name, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
	if (rx == (mqd_t) -1)
	{
		mq_close(tx);
		return 2;	
	}
	// SEND MESSAGE
	if (mq_send(tx, (char *)&msg, sizeof(struct message_t), 1) == (mqd_t) -1)
	{
		mq_close(tx);
		mq_close(rx);
		mq_unlink(rx_name);
		return 3;
	}
	int i = 0;
	do
	{
		// GET RESPONSE
		if (mq_receive(rx, (char *)&msg, sizeof(struct message_t), NULL) == (mqd_t) -1) {
			mq_close(tx);
			mq_close(rx);
			mq_unlink(rx_name);
			return 4;
		}
		if (msg.flag == CRON_FAILURE)
		{
			return 5;
		}
		if (msg.flag != CRON_SUCCESS)
		{
			printf("ID: %d | Command: %s | Args: %s | Time %.1lf | Type: %s\n", msg.pid, msg.program, msg.args, msg.seconds, msg.flag == CRON_TASK_STANDARD ? "Runs once" : "Runs periodically");
			i++;
		}
	} while (msg.flag != CRON_SUCCESS);
	if (i < 1)
	{
		printf("No tasks to display\n");
	}
	// CLOSE QUEUES
	mq_close(tx);
	mq_close(rx);
	mq_unlink(rx_name);
	// ALL OK
	return 0;
}

void timer_thread(union sigval arg)
{
	struct task_t *task = (struct task_t *)arg.sival_ptr;
	
	pid_t pid, sid;
	pid = fork();
	if (pid < 0)
	{
		return;
	}
	if (pid > 0)
	{
		logger_log("Program \'%s %s\' has been executed.\n", task->msg.program, task->msg.args);
		// CLEAR IF STANDARD
		if (task->msg.flag == CRON_TASK_STANDARD)
		{
			memset(task, 0, sizeof(struct task_t));
		}
		return;
	}
	umask(0);
	sid = setsid();
	if (sid < 0)
	{
		return;
	}

	char* args[MAX_STRING_LEN] = {task->msg.program, NULL};
	int i = 1;
	char *pch;
	pch = strtok(task->msg.args, " ");
	while (pch != NULL)
	{
		args[i] = pch;
		pch = strtok(NULL, " ");
		i++;
	}
	

	args[i] = NULL;
	execvp(task->msg.program, args);
}


int cron_server_loop()
{
	struct task_t task_list[MAX_TASK_LIST] = { 0 };

	// START LOGGER
	if (logger_start("cron_log.txt", task_list, sizeof(struct task_t) * MAX_TASK_LIST) != 0)
	{
		return 1;
	}
	logger_set_level(LOG_STD);

	// INIT QUEUE NAMES
	char tx_name[MAX_STRING_LEN];
	mqd_t rx = (mqd_t)-1;
	mqd_t tx = (mqd_t)-1;
	// FILL ATTRIBUTES
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = MAX_QUEUE_SIZE;
	attr.mq_msgsize = sizeof(struct message_t);
	attr.mq_curmsgs = 0;
	// CREATE SERVER QUEUE
	rx = mq_open(SERVER_QUEUE_NAME, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
	if (rx == (mqd_t)-1)
	{
		logger_log("Couldn't create server queue");
		logger_stop();
		return 2;
	}
	logger_log("Cron daemon started!");
	// MAIN MESSAGE QUEUE
	struct message_t received_msg = { 0 };
	while (1) {
		// WAIT FOR COMMAND
		if (mq_receive(rx, (char *)&received_msg, sizeof(struct message_t), NULL) != (mqd_t) -1)
		{
			sprintf(tx_name, "%s_%d", CLIENT_QUEUE_NAME, received_msg.pid);
			// OPEN CLIENT QUEUE
			tx = mq_open(tx_name, O_WRONLY);
			if (tx == (mqd_t)-1) {
				logger_log("Couldn't open client queue.");
				logger_stop();
				mq_close(rx);
				mq_unlink(SERVER_QUEUE_NAME);
				return 3;
			}
			// OUT CHECK MESSAGE
			struct message_t outgoing_msg = { 0 };
			outgoing_msg.flag = CRON_SUCCESS;
			// HANDLE MESSAGE
			switch (received_msg.flag)
			{
				case CRON_TASK_PERIODIC:
				case CRON_TASK_STANDARD:
				{
					int i = 0;
					for (; i < MAX_TASK_LIST; ++i)
					{
						if (task_list[i].timer == NULL)
						{
							break;
						}
					}
					if (i < MAX_TASK_LIST)
					{
						struct itimerspec ts = { 0 };
						ts.it_value.tv_sec = received_msg.seconds;
						if (received_msg.flag == CRON_TASK_PERIODIC)
						{	
							ts.it_interval.tv_sec = received_msg.seconds;
						}
						struct sigevent se;
						se.sigev_notify = SIGEV_THREAD;
						se.sigev_value.sival_ptr = &task_list[i];
						se.sigev_notify_function = timer_thread;
						se.sigev_notify_attributes = NULL;
						// CREATE TIMER
						int status = timer_create(CLOCK_REALTIME, &se, &task_list[i].timer);
						if (status == -1)
						{
							logger_log("Couldn't create timer.");
							outgoing_msg.flag = CRON_FAILURE;
						}
						status = timer_settime(task_list[i].timer, 0, &ts, 0);
						if (status == -1)
						{
							logger_log("Couldn't set timer.");
							outgoing_msg.flag = CRON_FAILURE;
						}
						task_list[i].msg = received_msg;
						task_list[i].msg.pid = i;
						logger_log("PID: %d has queued a new task: %s %s in %.0lf seconds.", received_msg.pid, received_msg.program, received_msg.args, received_msg.seconds);
					}
					else
					{
						logger_log("Can't queue more tasks.");
						outgoing_msg.flag = CRON_FAILURE;
					}
					break;
				}	
				case CRON_TASK_SHOW:
				{
					for (int i = 0; i < MAX_TASK_LIST; ++i)
					{
						if (task_list[i].timer != NULL)
						{
							mq_send(tx, (char *)&task_list[i].msg, sizeof(struct message_t), 1);
						}
					}
					logger_log("Send list of tasks to PID: %d.", received_msg.pid);
					break;
				}
				case CRON_TASK_CANCEL:
				{
					for (int i = 0; i < MAX_TASK_LIST; ++i)
					{
						if (task_list[i].timer != NULL && task_list[i].msg.pid == received_msg.seconds)
						{
							struct itimerspec ts = { 0 };
							int status = timer_settime(task_list[i].timer, 0, &ts, 0);
							if (status == -1)
							{
								logger_log("Couldn't disarm timer");
								outgoing_msg.flag = CRON_FAILURE;
							}
							logger_log("PID %d cancelled a task: %.0lf", received_msg.pid, received_msg.seconds);
							task_list[i].timer = NULL;
						}
					}
					break;
				}
			}
			// RESPOND
			mq_send(tx, (char *)&outgoing_msg, sizeof(struct message_t), 1);
			// CLOSE CLIENT QUEUE
			mq_close(tx);
			// QUIT IF NECESSARY
			if (received_msg.flag == CRON_QUIT)
			{
				break;
			}
		}
	}
	logger_log("Cron daemon stopped!");
	mq_close(rx);
	mq_unlink(SERVER_QUEUE_NAME);
	logger_stop();
	return 0;
}