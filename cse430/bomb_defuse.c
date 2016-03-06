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

static unsigned short fork_rate = CONFIG_BOMB_DEFUSE_RATE;
module_param(fork_rate, ushort, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(fork_rate, "Forks per second for bomb detection.");

static unsigned int sleep_time = CONFIG_BOMB_DEFUSE_TIME;
module_param(sleep_time, uint, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(sleep_time, "How often processes are checked for fork bombs.");

static void check_for_bombs(void)
{
	char comm[TASK_COMM_LEN];
	u64 p_time;
	struct task_struct *p;
	rcu_read_lock();
	for_each_process(p) {
		/* exempt init */
		if (is_global_init(p))
			continue;
		/* exempt kernel threads */
		if (p->flags & PF_KTHREAD)
			continue;
		p_time = (ktime_get_ns() - p->start_time) / NSEC_PER_SEC;
		/* process just started */
		if (p_time == 0)
			continue;
		/* process has been running for too long to be a fork bomb */
		if (p_time > (2 * sleep_time))
			continue;
		/* process has not been forking enough to be a fork bomb */
		if (p->forks < (fork_rate * p_time))
			continue;
		get_task_comm(comm, p);
		pr_info("bomb_defuse: killing %d %s\n", task_pid_nr(p), comm);
		/* kill fork bomb process and its children */
		kill_pgrp(task_pgrp(p), SIGKILL, 1);
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
