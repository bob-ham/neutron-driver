#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/mman.h>
#include <asm/mman.h>
#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/tlbflush.h>
#include "core.h"
#include "neutron.h"
#include "support.h"



#define CHUNK_SIZE 4096

// --------------------
// TASK HELPERS
// --------------------

static struct task_struct *get_target_task(pid_t pid)
{
    struct task_struct *task = NULL;
    struct pid *pid_struct;

    rcu_read_lock();
    pid_struct = find_vpid(pid);

    if (pid_struct)
    {
        task = pid_task(pid_struct, PIDTYPE_PID);
        if (task)
        {
            get_task_struct(task);
        }
    }
    rcu_read_unlock();
    return task;
}

// -----------------------
// STOP/CONT SECTION
// -----------------------

static int impl_stop_proc(uint32_t pid)
{
    struct task_struct *task = get_target_task(pid);
    if (!task) return -ESRCH;

    // send SIGSTOP to pause the process
    send_sig(SIGSTOP, task, 1);

    put_task_struct(task);
    return 0;
}

static int impl_cont_proc(uint32_t pid)
{
    struct task_struct *task = get_target_task(pid);
    if (!task) return -ESRCH;

    // send SIGCONT to resume process
    send_sig(SIGCONT, task, 1);

    put_task_struct(task);
    return 0;
}

// -----------------------
// MPROTECT SECTION
// -----------------------

static int impl_set_protection(uint32_t pid, uint64_t address, uint64_t length, uint32_t mode)
{
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    vm_flags_t new_flags;

    task = get_target_task(pid);
    if (!task) return -ESRCH;

    mm = get_task_mm(task);
    if (!mm) { 
        put_task_struct(task); 
        return -ESRCH; 
    }

    mmap_write_lock(mm);

    // locate the vma containing the target address
    vma = find_vma(mm, (unsigned long)address);

    // safeguard 1 verify vma actually covers the requested address range
    if (!vma || vma->vm_start > address)
    {
        pr_warn("neutron: attempted to change prot of unmapped memory at 0x%llx\n", address);
        mmap_write_unlock(mm);
        mmput(mm);
        put_task_struct(task);
        return -EFAULT;
    }

    // safeguard 2 verify the requested length is within this single vma (including overflow check)
    if (address + length < address || (address + length) > vma->vm_end)
    {
        pr_warn("neutron: protection range spans across multiple VMAs or overflows, aborting\n");
        mmap_write_unlock(mm);
        mmput(mm);
        put_task_struct(task);
        return -EINVAL;
    }

    // safeguard 3 prevent modification of kernel space boundaries
    if (address < 0x400000 || address > 0x7fffffffffff) 
    {
        pr_err("neutron: protection request on invalid address range 0x%llx\n", address);
        mmap_write_unlock(mm);
        mmput(mm);
        put_task_struct(task);
        return -EPERM;
    }

    // Apply changes safely using modern kernel reset API while preserving other flags
    new_flags = vma->vm_flags;
    new_flags &= ~(VM_READ | VM_WRITE | VM_EXEC | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC);
    if (mode == NEUTRON_PROT_RO) {
        new_flags |= (VM_READ | VM_MAYREAD);
    } else if (mode == NEUTRON_PROT_RW) {
        new_flags |= (VM_READ | VM_WRITE | VM_MAYREAD | VM_MAYWRITE);
    } else if (mode == NEUTRON_PROT_RWX) {
        new_flags |= (VM_READ | VM_WRITE | VM_EXEC | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC);
    }

    vm_flags_reset(vma, new_flags);

    mmap_write_unlock(mm);
    mmput(mm);
    put_task_struct(task);
    return 0;
}

// -----------------------
// MEMORY SECTION
// -----------------------

static int impl_read_mem(uint32_t pid, uint64_t target_address, uint64_t local_buffer, uint64_t size)
{
    struct task_struct *task;
    void *kernel_buf;
    uint64_t bytes_remaining;
    uint64_t current_target = target_address;
    uint64_t current_local = local_buffer;
    int total_bytes_Read = 0;

    if (size > INT_MAX) return -EINVAL;
    bytes_remaining = size;

    task = get_target_task(pid);
    if (!task) return -ESRCH;

    kernel_buf = kmalloc(CHUNK_SIZE, GFP_KERNEL);
    if (!kernel_buf)
    {
        put_task_struct(task);
        return -ENOMEM;
    }

    while (bytes_remaining > 0)
    {
        uint64_t to_read = (bytes_remaining > CHUNK_SIZE) ? CHUNK_SIZE : bytes_remaining;
        int bytes_read = access_process_vm(task, current_target, kernel_buf, to_read, 0);
        if (bytes_read <= 0)
        {
            break;
        }

        if (copy_to_user((void __user *)current_local, kernel_buf, bytes_read))
        {
            total_bytes_Read = -EFAULT;
            break;
        }

        total_bytes_Read += bytes_read;
        current_target += bytes_read;
        current_local += bytes_read;
        bytes_remaining -= bytes_read;

        if (bytes_read < to_read)
        {
            break;
        }
    }

    kfree(kernel_buf);
    put_task_struct(task);
    return total_bytes_Read;
}

static int impl_write_mem(uint32_t pid, uint64_t target_address, uint64_t local_buffer, uint64_t size)
{
    struct task_struct *task;
    void *kernel_buf;
    uint64_t bytes_remaining;
    uint64_t current_target = target_address;
    uint64_t current_local = local_buffer;
    int total_bytes_written = 0;

    if (size > INT_MAX) return -EINVAL;
    bytes_remaining = size;

    task = get_target_task(pid);
    if (!task) return -ESRCH; 

    kernel_buf = kmalloc(CHUNK_SIZE, GFP_KERNEL);
    if (!kernel_buf)
    {
        put_task_struct(task);
        return -ENOMEM;
    }

    while (bytes_remaining > 0)
    {
        uint64_t to_write = (bytes_remaining > CHUNK_SIZE) ? CHUNK_SIZE : bytes_remaining;

        if (copy_from_user(kernel_buf, (void __user *)current_local, to_write))
        {
            total_bytes_written = -EFAULT;
            break;
        }

        int bytes_written = access_process_vm(task, current_target, kernel_buf, to_write, 1);
        if (bytes_written <= 0)
        {
            break;
        }

        total_bytes_written += bytes_written;
        current_target += bytes_written;
        current_local += bytes_written;
        bytes_remaining -= bytes_written;

        if (bytes_written < to_write)
        {
            break;
        }
    }

    kfree(kernel_buf);
    put_task_struct(task);
    return total_bytes_written;
}

static int impl_query_mem(uint32_t pid, uint64_t *address, uint64_t *size, uint32_t *protection)
{
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    unsigned long query_addr = (unsigned long)*address;

    task = get_target_task(pid);
    if (!task) return -ESRCH;

    mm = get_task_mm(task);
    if (!mm)
    {
        put_task_struct(task);
        return -ESRCH;
    }

    mmap_read_lock(mm);
    vma = find_vma(mm, query_addr);

    if (!vma)
    {
        mmap_read_unlock(mm);
        mmput(mm);
        put_task_struct(task);
        return -ENOENT;
    }

    *address = (uint64_t)vma->vm_start;
    *size    = (uint64_t)(vma->vm_end - vma->vm_start);

    *protection = 0;
    if (vma->vm_flags & VM_READ)  *protection |= 1; 
    if (vma->vm_flags & VM_WRITE) *protection |= 2; 
    if (vma->vm_flags & VM_EXEC)  *protection |= 4; 

    mmap_read_unlock(mm);
    mmput(mm);
    put_task_struct(task);
    return 0;
}

struct mmap_work {
    struct task_struct *target_task;
    unsigned long addr;
    unsigned long len;
    unsigned long prot;
    unsigned long flags;
    unsigned long ret_addr;
    struct completion done;
};

static int mmap_kthread_func(void *data) {
    struct mmap_work *work = data;
    struct mm_struct *mm = get_task_mm(work->target_task);
    if (mm) {
        kthread_use_mm(mm);
        work->ret_addr = vm_mmap(NULL, work->addr, work->len, work->prot, work->flags, 0);
        kthread_unuse_mm(mm);
        mmput(mm);
    } else {
        work->ret_addr = -ESRCH;
    }
    complete(&work->done);
    return 0;
}


static int impl_allocate_mem(uint32_t pid, uint64_t *address, uint64_t size, uint32_t prot_mode, uint32_t flags)
{
    struct task_struct *task;
    struct mmap_work work;
    struct task_struct *kthread;
    unsigned long prot = 0;

    // Convert NEUTRON_PROT flags to kernel protection flags
    if (prot_mode == NEUTRON_PROT_RO) prot = PROT_READ;
    else if (prot_mode == NEUTRON_PROT_RW) prot = PROT_READ | PROT_WRITE;
    else if (prot_mode == NEUTRON_PROT_RWX) prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    else return -EINVAL;

    task = get_target_task(pid);
    if (!task) return -ESRCH;

    init_completion(&work.done);
    work.target_task = task;
    work.addr = (unsigned long)*address;
    work.len = (unsigned long)size;
    work.prot = prot;
    work.flags = MAP_PRIVATE | MAP_ANONYMOUS;
    work.ret_addr = 0;

    kthread = kthread_run(mmap_kthread_func, &work, "neutron_mmap");
    if (IS_ERR(kthread)) {
        put_task_struct(task);
        return PTR_ERR(kthread);
    }

    wait_for_completion(&work.done);
    put_task_struct(task);

    if (IS_ERR_VALUE(work.ret_addr)) {
        return (int)work.ret_addr;
    }

    *address = (uint64_t)work.ret_addr;
    return 0;
}

struct munmap_work {
    struct task_struct *target_task;
    unsigned long addr;
    unsigned long len;
    int ret;
    struct completion done;
};

static int munmap_kthread_func(void *data) {
    struct munmap_work *work = data;
    struct mm_struct *mm = get_task_mm(work->target_task);
    if (mm) {
        kthread_use_mm(mm);
        work->ret = vm_munmap(work->addr, work->len);
        kthread_unuse_mm(mm);
        mmput(mm);
    } else {
        work->ret = -ESRCH;
    }
    complete(&work->done);
    return 0;
}

static int impl_free_mem(uint32_t pid, uint64_t address, uint64_t size)
{
    struct task_struct *task;
    struct munmap_work work;
    struct task_struct *kthread;

    task = get_target_task(pid);
    if (!task) return -ESRCH;

    init_completion(&work.done);
    work.target_task = task;
    work.addr = (unsigned long)address;
    work.len = (unsigned long)size;
    work.ret = 0;

    kthread = kthread_run(munmap_kthread_func, &work, "neutron_munmap");
    if (IS_ERR(kthread)) {
        put_task_struct(task);
        return PTR_ERR(kthread);
    }

    wait_for_completion(&work.done);
    put_task_struct(task);

    return work.ret;
}

struct neutron_core_context neutron_core = {
    .memory = {
        .set_protection = impl_set_protection,
        .read_mem       = impl_read_mem,
        .write_mem      = impl_write_mem,
        .query_mem      = impl_query_mem,
        .allocate_mem   = impl_allocate_mem,
        .free_mem       = impl_free_mem,
    },
    .env   = {
        .stop           = impl_stop_proc,
        .cont           = impl_cont_proc,
    },
};