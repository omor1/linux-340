#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <asm/pgtable.h>

#define PAGE_SWAP_BITS		62
#define PAGE_SWAP		(1ULL << PAGE_SWAP_BITS)

asmlinkage long sys_get_addr(pid_t nr, unsigned long addr,
			unsigned long long __user *phys)
{
	long ret = 0;
	struct task_struct *p;
	spinlock_t *ptl;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long long kphys;

	rcu_read_lock();
	p = find_task_by_vpid(nr);
	rcu_read_unlock();

	/* if p NULL or p is dead or a zombie */
	if (!p || (p->exit_state & (EXIT_DEAD | EXIT_ZOMBIE))) {
		ret = -ESRCH;
		goto out;
	}

	/* if p is a kernel thread, use kernel mm (kernel thread mm is NULL) */
	pgd = p->mm ? pgd_offset(p->mm, addr) : pgd_offset_k(addr);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
		ret = -EINVAL;
		goto out;
	}

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud) || unlikely(pud_bad(*pud))) {
		ret = -EINVAL;
		goto out;
	}

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd))) {
		ret = -EINVAL;
		goto out;
	}

	pte = pte_offset_map_lock(p->mm, pmd, addr, &ptl);

	// unlikely to be in swap
	if (unlikely(is_swap_pte(*pte)))
		kphys = PAGE_SWAP | pte_to_swp_entry(*pte).val;
	else
		kphys = (pte_val(*pte) & PTE_PFN_MASK) | (addr & (PAGE_SIZE-1));

	pte_unmap_unlock(pte, ptl);

	if(copy_to_user(phys, &kphys, sizeof(kphys)))
		ret = -EFAULT;

out:
	return ret;
}
