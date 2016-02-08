#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
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
	long ret = 0;
	struct task_info *kbuf = kmalloc_array(len, sizeof(*kbuf), GFP_KERNEL);
	size_t i = 0;
	struct task_struct *task;
	for_each_process(task) {
		if (i >= len) {
			ret = -ENOBUFS;
			break;
		}
		kbuf[i].pid = task_pid_nr(task);
		tty_name(task->signal->tty, kbuf[i].tty_name);
		cputime_t utime, stime;
		thread_group_cputime_adjusted(task, &utime, &stime);
		kbuf[i].nsecs = cputime_to_nsecs(utime + stime);
		get_task_comm(kbuf[i].comm, task);
		i++;
	}
	if (i < len)
		ret = -E2BIG;
	copy_to_user(buf, kbuf, sizeof(*buf) * len);
	kfree(kbuf);
	return ret;
}
