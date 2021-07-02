# Cron
Implementation of a time-based job scheduler based on Cron. It allows to schedule programs in a given time.

## Prerequisites
* UNIX-based operating system with support for POSIX.

## Description
To start cron daemon, run program with `-s` argument. It will create a new process which logs it's actions to `cron_log.txt`. To display all available options, use `-h` argument.

### Scheduling tasks
Following example will call a program from `$PATH` once on a fixed date with an argument `done`.
```
./cron -p 23/07/2021 14:32:11 spd-say done
```
This call will execute local program every 30 seconds with an argument `arg`.
```
./cron -p 30 -r /media/USB/sample-program arg
```
Last example will execute a program every day if system date predeceases given date by a day.
```
./cron -p 02/07/2021 00:00:00 -r ls -la
```
### Cancelling tasks
In order to cancel previously scheduled task, it's necessary to get it's ID by calling:
```
./cron -g
ID: 0 | Command: spd-say | Args: good  | Time 3.0 | Type: Runs periodically
ID: 1 | Command: ls | Args: la  | Time 350.0 | Type: Runs once
ID: 2 | Command: reboot | Args:  | Time 36000 | Type: Runs periodically
```
To cancel a task of choice:
```
./cron -c 2
```
### Remarks
Deamon process is detached from `stdout`, `stdin` and `stderr`. Scheduled tasks should not depend on those file descriptors.

Working directory of scheduled tasks is not changed. Therefore, all files created by tasks are stored in the same directory as cron binary.

It's important to stop cron deamon by calling it with `-q` paramenter. Otherwise, cron might hang the system upon reboot.

## Building
Command to build cron:
```
gcc cron.c main.c -o cron -lpthread -lrt
```