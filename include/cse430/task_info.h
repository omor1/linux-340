#ifndef _CSE430_TASK_INFO_H
#define _CSE430_TASK_INFO_H

#include <linux/types.h>
#include <linux/sched.h>

#define TTY_NAME_LEN 64

struct task_info {
	pid_t pid;
	char tty_name[TTY_NAME_LEN];
	unsigned long long nsecs;
	char comm[TASK_COMM_LEN];
};

#endif /* _CSE430_TASK_INFO_H */
