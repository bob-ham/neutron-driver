#ifndef NEUTRON_SUPPORT_H
#define NEUTRON_SUPPORT_H

#include <linux/version.h>
#include <linux/mm.h>
#include <linux/sched.h>

/*
 * Compatibility wrapper for vm_flags_reset (introduced in 6.3)
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
static inline void vm_flags_reset(struct vm_area_struct *vma, vm_flags_t flags)
{
    vma->vm_flags = flags;
}
#endif

/*
 * Compatibility wrappers for mmap locking APIs (introduced in 5.8)
 */


#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
#define mmap_write_lock(mm)     down_write(&(mm)->mmap_sem)
#define mmap_write_unlock(mm)   up_write(&(mm)->mmap_sem)
#define mmap_read_lock(mm)      down_read(&(mm)->mmap_sem)
#define mmap_read_unlock(mm)    up_read(&(mm)->mmap_sem)

#define kthread_use_mm(mm)      use_mm(mm)
#define kthread_unuse_mm(mm)    unuse_mm(mm)
#endif

/*
 * Compatibility wrapper for class_create (changed in 6.4)
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
#define class_create(name)      class_create(THIS_MODULE, name)
#endif

#endif /* NEUTRON_SUPPORT_H */

