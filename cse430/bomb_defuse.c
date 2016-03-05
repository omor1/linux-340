#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/timekeeping.h>

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Omri Mor <Omri.Mor@asu.edu>");
MODULE_AUTHOR("Derek Jones <DerekJones@asu.edu>");
MODULE_AUTHOR("Brett Caley <Brett.Caley@asu.edu>");
MODULE_DESCRIPTION("defuse fork bombs");

#ifndef CONFIG_BOMB_DEFUSE_RATE
#define CONFIG_BOMB_DEFUSE_RATE 20
#endif

#ifndef CONFIG_BOMB_DEFUSE_TIME
#define CONFIG_BOMB_DEFUSE_TIME 10
#endif

static unsigned short fork_rate = CONFIG_BOMB_DEFUSE_RATE;
module_param(fork_rate, ushort, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(fork_rate, "Forks per second for bomb detection.");

static unsigned int sleep_time = CONFIG_BOMB_DEFUSE_TIME;
module_param(sleep_time, uint, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(sleep_time, "How often processes are checked for fork bombs.");

static inline int is_bomb(struct task_struct *task)
{
	u64 task_time = (ktime_get_ns() - task->start_time) / NSEC_PER_SEC;
	return !is_global_init(task) && !(task->flags & PF_KTHREAD) &&
			(task_time < (2 * sleep_time)) && (task_time > 0) &&
			(task->forks > (fork_rate * task_time));
}

static void check_for_bombs(void)
{
	char comm[TASK_COMM_LEN];
	struct task_struct *task;
	rcu_read_lock();
	for_each_process(task) {
		if (!is_bomb(task))
			continue;
		get_task_comm(comm, task);
		printk(KERN_INFO "killing fork bomb: %d %s\n",
				task_pid_nr(task), comm);
		kill_pgrp(task_pgrp(task), SIGKILL, 0);
	}
	rcu_read_unlock();
}

static int bomb_defuse(void *unused)
{
	while (!kthread_should_stop()) {
		check_for_bombs();
		ssleep(sleep_time);
	}
	return 0;
}

static struct task_struct *bomb_defuse_task;

static int __init bomb_defuse_init(void)
{
	bomb_defuse_task = kthread_run(bomb_defuse, NULL, "bomb_defuse");
	return PTR_ERR_OR_ZERO(bomb_defuse_task);
}

static void __exit bomb_defuse_exit(void)
{
	if (!IS_ERR(bomb_defuse_task))
		kthread_stop(bomb_defuse_task);
}

module_init(bomb_defuse_init);
module_exit(bomb_defuse_exit);
