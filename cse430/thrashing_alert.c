#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <asm/pgtable.h>

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Omri Mor <Omri.Mor@asu.edu>");
MODULE_AUTHOR("Derek Jones <DerekJones@asu.edu>");
MODULE_AUTHOR("Brett Caley <Brett.Caley@asu.edu>");
MODULE_DESCRIPTION("alert when thrashing");

static unsigned int sleep_time = 1;
module_param(sleep_time, uint, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(sleep_time, "How often the working set size is checked.");

static size_t calculate_working_set(struct mm_struct *mm)
{
	size_t wss = 0;
	struct vm_area_struct *vma;
	unsigned long addr;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	down_read(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		for (addr = vma->vm_start; addr < vma->vm_end;
					   addr += PAGE_SIZE) {
			pgd = pgd_offset(mm, addr);
			if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
				continue;
			pud = pud_offset(pgd, addr);
			if (pud_none(*pud) || unlikely(pud_bad(*pud)))
				continue;
			pmd = pmd_offset(pud, addr);
			if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd)))
				continue;
			pte = pte_offset_map(pmd, addr);
			if (!pte_none(*pte) && pte_present(*pte) &&
					       pte_young(*pte)) {
				wss++;
				set_pte(pte, pte_mkold(*pte));
			}
			pte_unmap(pte);
		}
	}
	up_read(&mm->mmap_sem);
	return wss;
}

static int thrashing_alert(void *unused)
{
	struct task_struct *p;
	size_t twss, wss;

	while (!kthread_should_stop()) {
		twss = 0;
		rcu_read_lock();
		for_each_process(p) {
			if (p->mm) {
				wss = calculate_working_set(p->mm);
				pr_info("[%d] : [%zu]\n", p->pid, wss);
				twss += wss;
			}
		}
		rcu_read_unlock();
		pr_info("[Total WSS] : [%zu]\n", twss);
		if (10 * twss > 9 * totalram_pages)
			pr_warning("Kernel Alert!\n");
		ssleep(sleep_time);
	}
	return 0;
}

static struct task_struct *thrashing_alert_task;

static int __init thrashing_alert_init(void)
{
	thrashing_alert_task = kthread_run(thrashing_alert, NULL,
				"thrashing alert");
	return PTR_ERR_OR_ZERO(thrashing_alert_task);
}

static void __exit thrashing_alert_exit(void)
{
	if (!IS_ERR(thrashing_alert_task))
		kthread_stop(thrashing_alert_task);
}

module_init(thrashing_alert_init);
module_exit(thrashing_alert_exit);
