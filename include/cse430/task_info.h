#ifndef _CSE430_TASK_INFO_H
#define _CSE430_TASK_INFO_H

#include <linux/types.h>
#include <linux/sched.h>

struct task_info {
	pid_t pid;
	char tty_name[64];
	size_t secs;
	char comm[TASK_COMM_LEN];
};

#endif /* _CSE430_TASK_INFO_H */
