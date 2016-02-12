#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/uaccess.h>

#include <cse430/task_info.h>

asmlinkage long sys_ntasks(size_t __user *num)
{
	size_t knum = 0;
	struct task_struct *task;
	for_each_process(task)
		knum++;
	copy_to_user(num, &knum, sizeof(*num));
	return 0;
}

asmlinkage long sys_task_info(struct task_info __user *buf, size_t len)
{
	ssize_t ret = len;
	struct task_info ktask_info;
	cputime_t utime, stime;
	size_t i = 0;
	struct task_struct *task;
	for_each_process(task) {
		if (i >= len) {
			ret = -ENOBUFS;
			break;
		}
		ktask_info.pid = task_pid_nr(task);
		tty_name(task->signal->tty, ktask_info.tty_name);
		thread_group_cputime_adjusted(task, &utime, &stime);
		ktask_info.nsecs = cputime_to_nsecs(utime + stime);
		get_task_comm(ktask_info.comm, task);
		copy_to_user(&buf[i], &ktask_info, sizeof(*buf));
		i++;
	}
	if (i < len)
		ret = i;
	return ret;
}
