#ifndef CORE_H
#define CORE_H

#include <linux/types.h>

struct memory_operations
{
    int (*set_protection) (uint32_t pid, uint64_t address, uint64_t length, uint32_t mode);
    int (*read_mem)       (uint32_t pid, uint64_t target_address, uint64_t local_buffer, uint64_t size);
    int (*write_mem)      (uint32_t pid, uint64_t target_address, uint64_t local_buffer, uint64_t size);
    int (*query_mem)      (uint32_t pid, uint64_t *address, uint64_t *size, uint32_t *protection);
    int (*allocate_mem)   (uint32_t pid, uint64_t *address, uint64_t size, uint32_t prot, uint32_t flags);
    int (*free_mem)       (uint32_t pid, uint64_t address, uint64_t size);
};

struct environment_operations
{
    int (*stop)           (uint32_t pid);
    int (*cont)           (uint32_t pid);
};

struct neutron_core_context
{
    struct memory_operations      memory;
    struct environment_operations  env;
};

extern struct neutron_core_context neutron_core;

#endif