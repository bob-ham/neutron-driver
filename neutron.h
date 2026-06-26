#ifndef NEUTRON_H
#define NEUTRON_H

#ifdef __KERNEL__
    #include <linux/types.h>
    #include <linux/ioctl.h>
#else
    #include <linux/types.h>
    #include <sys/ioctl.h>
#endif

// magic number
#define NEUTRON_MAGIC 'n'

// device name
#define DEVICE_NAME "neutron"

// flags
// #define NEUTRON_HIDE_THREAD     0x00000001
// #define NEUTRON_VERBOSE         0x00000002
// #define NEUTRON_UNSAFE          0x00000004

// no point for any of these flags since we arent doing anything that can crash the OS

// prot modes for MPROTECT
#define NEUTRON_PROT_RW     0x01
#define NEUTRON_PROT_RO     0x02
#define NEUTRON_PROT_RWX    0x03

// structs for IOCTL

// mprotect struct
struct __attribute__((packed)) neutron_prot_context
{
    __u32 target_pid;
    __u32 protection_mode;
    __u64 target_addr;
    __u64 length;
};

// thread struct
struct __attribute__((packed)) neutron_thread_context
{
    __u32 target_pid;
    __u32 flags;
    __u64 entry_point;
};

// read/write regs struct
struct __attribute__((packed)) neutron_mem_context
{
    __u32 target_pid;
    __u64 target_address;
    __u64 local_buffer;
    __u64 size;
};

struct __attribute__((packed)) neutron_mem_query
{
    __u32 target_pid;
    __u64 address;
    __u64 size;
    __u32 protection;
};

// injection struct
struct __attribute__((packed)) neutron_inject_context
{
    __u32 target_pid;
    __u32 flags;
    char so_path[256];
};

// virtual memory struct
struct __attribute__((packed)) neutron_vmem_context
{
    __u32 target_pid;
    __u32 prot;
    __u64 address;
    __u64 size;
    __u32 flags;
    __u32 status;
};

// ioctl commands defs


// complex ioctl commands
#define NEUTRON_CREATE_THREAD  _IOW(NEUTRON_MAGIC,  0x01, struct neutron_thread_context) // <- creates a thread 
#define NEUTRON_READ_MEMORY    _IOWR(NEUTRON_MAGIC, 0x02, struct neutron_mem_context)   // <- reads CPU regs
#define NEUTRON_WRITE_MEMORY   _IOWR(NEUTRON_MAGIC,  0x03, struct neutron_mem_context)   // <- writes CPU regs (requires _UNSAFE flag to write to RIP or anything important)
#define NEUTRON_QUERY_MEMORY   _IOWR(NEUTRON_MAGIC, 0x05, struct neutron_mem_query)
#define NEUTRON_MPROTECT       _IOW(NEUTRON_MAGIC,  0x07, struct neutron_prot_context)   // <- changes prot of a memory region (so RO to RWX or RW to RX)
#define NEUTRON_CORE_INJECT    _IOW(NEUTRON_MAGIC,  0x0A, struct neutron_inject_context) // <- forcefully adds a .so library to a process (requires _UNSAFE flag to use)
#define NEUTRON_VIRTUAL_MEMD   _IOWR(NEUTRON_MAGIC, 0x0B, struct neutron_vmem_context)
#define NEUTRON_VIRTUAL_MEMFR  _IOWR(NEUTRON_MAGIC, 0x0C, struct neutron_vmem_context)

// ioctl commands that use one arguement
// #define NEUTRON_ATTACH        _IOW(NEUTRON_MAGIC, 0x04, __u32)
// #define NEUTRON_DETACH        _IOW(NEUTRON_MAGIC, 0x05, __u32)
// not needed, got rid of them and instead will use them for NEUTRON_VIRT

#define NEUTRON_POKEHOLE      _IOW(NEUTRON_MAGIC, 0x06, __u32) // <- pokes a hole in a sandboxed envirorment like a flatpak
#define NEUTRON_STOP_PROC     _IOW(NEUTRON_MAGIC, 0x08, __u32)
#define NEUTRON_CONT_PROC     _IOW(NEUTRON_MAGIC, 0x09, __u32)

#endif