#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h> 
#include "neutron.h"
#include "core.h"
#include "support.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("freakster");
MODULE_DESCRIPTION("neutron core for happiness");
MODULE_VERSION("1.0");

static dev_t dev_num;
static struct cdev cdev;
static struct class *neutron_class;
static struct device *neutron_device; 




// ----------------------------------------------
// MAIN IOCTL CASE
// added _MPROTECT, _READ, _WRITE, _STOP, _CONT, _ATTACH, _DETACH (7)
// added flags _HIDE_THREAD, _VERBOSE, _UNSAFE (3)
// need to add flag _HIDE_THREAD (1)
// need to add _AOB_SCAN, _POKEHOLE, _QUERY_MEMORY, 
// _VIRTUAL_MEMD, _VIRTUAL_MEMFR, _CORE_INJECT, _CREATE_THREAD, _HIJACK_THREAD (8)
// ----------------------------------------------

static long neutron_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case NEUTRON_MPROTECT: { // 1 <- marks how many calls ive created
            struct neutron_prot_context ctx;
            if (copy_from_user(&ctx, (void __user *)arg, sizeof(ctx))) return -EFAULT;


            return neutron_core.memory.set_protection(ctx.target_pid, ctx.target_addr, ctx.length, ctx.protection_mode);
        }
        case NEUTRON_READ_MEMORY: { // 2
            struct neutron_mem_context ctx;
            if (copy_from_user(&ctx, (void __user *)arg, sizeof(ctx)))
                return -EFAULT;
            
            return neutron_core.memory.read_mem(ctx.target_pid, ctx.target_address, ctx.local_buffer, ctx.size);
        }
        case NEUTRON_WRITE_MEMORY: { // 3
            struct neutron_mem_context ctx;
            if (copy_from_user(&ctx, (void __user *)arg, sizeof(ctx)))
                return -EFAULT;

            return neutron_core.memory.write_mem(ctx.target_pid, ctx.target_address, ctx.local_buffer, ctx.size);
        }
        case NEUTRON_QUERY_MEMORY: {
            struct neutron_mem_query ctx;
            long ret;

            if (copy_from_user(&ctx, (void __user *)arg, sizeof(ctx)))
                return -EFAULT;

            ret = neutron_core.memory.query_mem(ctx.target_pid, &ctx.address, &ctx.size, &ctx.protection);
            if (ret < 0) return ret;

            if (copy_to_user((void __user *)arg, &ctx, sizeof(ctx)))
                return -EFAULT;

            return 0;
    
        }
        case NEUTRON_STOP_PROC: { // 6
            __u32 pid;
            if (copy_from_user(&pid, (void __user *)arg, sizeof(pid))) return -EFAULT;
            return neutron_core.env.stop(pid);
        }
        case NEUTRON_CONT_PROC: { // 7
            __u32 pid;
            if (copy_from_user(&pid, (void __user *)arg, sizeof(pid))) return -EFAULT;
            return neutron_core.env.cont(pid);
        }
        case NEUTRON_VIRTUAL_MEMD: {
            struct neutron_vmem_context ctx;
            long ret;

            if (copy_from_user(&ctx, (void __user *)arg, sizeof(ctx)))
                return -EFAULT;

            ret = neutron_core.memory.allocate_mem(ctx.target_pid, &ctx.address, ctx.size, ctx.prot, ctx.flags);
            if (ret < 0) return ret;

            if (copy_to_user((void __user *)arg, &ctx, sizeof(ctx)))
                return -EFAULT;

            return 0;
        }
        case NEUTRON_VIRTUAL_MEMFR: {
            struct neutron_vmem_context ctx;
            long ret;

            if (copy_from_user(&ctx, (void __user *)arg, sizeof(ctx)))
                return -EFAULT;

            ret = neutron_core.memory.free_mem(ctx.target_pid, ctx.address, ctx.size);
            if (ret < 0) return ret;

            return 0;
        }
        default: return -EINVAL;
    }
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = neutron_ioctl,
};

// -----------------------
// __INIT FUNCTION
// -----------------------
static int __init neutron_init(void) {
    int ret;
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;

    // character device creation (cdev)
    cdev_init(&cdev, &fops);
    ret = cdev_add(&cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    neutron_class = class_create("neutron_class");
    if (IS_ERR(neutron_class)) {
        cdev_del(&cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(neutron_class);
    }

    neutron_device = device_create(neutron_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(neutron_device)) {
        class_destroy(neutron_class);
        cdev_del(&cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(neutron_device);
    }

    pr_info("neutron: driver initialized (Major: %d)\n", MAJOR(dev_num));
    return 0;
}

// -----------------------
// __EXIT FUNCTION
// -----------------------

static void __exit neutron_exit(void) {
    device_destroy(neutron_class, dev_num); 
    class_destroy(neutron_class);
    cdev_del(&cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("neutron: unloaded\n");
}

module_init(neutron_init);
module_exit(neutron_exit);