#pragma once

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <cstdint>
#include <sys/ioctl.h>
#include <stdexcept>
#include "../neutron.h"


// ----------------------------------------------
// added _MPROTECT, _READ, _WRITE, _STOP, _CONT, _ATTACH, _DETACH (7)
// added flags _HIDE_THREAD, _VERBOSE, _UNSAFE (3) [NOTE _CREATE_THREAD and _HIDE_THREAD ARE GONE]
// need to add flag _HIDE_THREAD (1)
// need to add _AOB_SCAN, _POKEHOLE, _QUERY_MEMORY (added _QUERY_MEM in 6-21-26), 
// _VIRTUAL_MEMD, _VIRTUAL_MEMFR, _CORE_INJECT (6)
// ----------------------------------------------


namespace NEUTRON
{
    class Driver
    {
    private:
        int m_fd;
    public:
        Driver()
        {
            m_fd = open("/dev/neutron", O_RDWR);
            if (m_fd < 0)
            {
                throw std::runtime_error("failed to open /dev/neutron. are you root?");
            }
        }
        ~Driver()
        {
            if (m_fd >= 0) close(m_fd);
        }
        
        // -----------------------
        // PROT SECTION
        // -----------------------


        // _set_protection <- changes a memory's PROT bits (RO, RW, RWX)
        void _set_protection(uint32_t pid, uint64_t addr, uint64_t len, uint32_t mode) // 1 <- counting how many wrappers i made
        {
            neutron_prot_context ctx;
            ctx.target_pid = pid;
            ctx.target_addr = addr;
            ctx.length = len;
            ctx.protection_mode = mode;

            if (ioctl(m_fd, NEUTRON_MPROTECT, &ctx) < 0)
            {
                throw std::runtime_error("NEUTRON_MPROTECT failed");
            }
        }

        // -----------------------
        // MEMORY SECTION
        // -----------------------

        int _read_mem(uint32_t pid, uint64_t addr, uint64_t local_buffer, uint64_t size) // 2
        {
            neutron_mem_context ctx;
            ctx.target_pid = pid;
            ctx.target_address = addr;
            ctx.local_buffer = local_buffer;
            ctx.size = size;

            int ret = ioctl(m_fd, NEUTRON_READ_MEMORY, &ctx);
            if (ret < 0)
            {
                throw std::runtime_error("NEUTRON_READ_MEM failed");
            }
            return ret;
        }
        
        int _write_mem(uint32_t pid, uint64_t addr, uint64_t local_buffer, uint64_t size) // 3
        {
            neutron_mem_context ctx;
            ctx.target_pid = pid;
            ctx.target_address = addr;
            ctx.local_buffer = local_buffer;
            ctx.size = size;

            int ret = ioctl(m_fd, NEUTRON_WRITE_MEMORY, &ctx);
            if (ret < 0)
            {
                throw std::runtime_error("NEUTRON_WRITE_MEM failed");
            }
            return ret;
        }

        bool _query_mem(uint32_t pid, uint64_t &addr, uint64_t &out_size, uint32_t &out_protection)
        {
            neutron_mem_query ctx;
            ctx.target_pid = pid;
            ctx.address = addr;
            ctx.size = 0;
            ctx.protection = 0;
        
            int result = ioctl(m_fd, NEUTRON_QUERY_MEMORY, &ctx);
            if (result < 0)
            {
                if (errno == ENOENT) {
                    return false; 
                }
                throw std::runtime_error("NEUTRON_QUERY_MEM failed critically");
            }
            addr = ctx.address;
            out_size = ctx.size;
            out_protection = ctx.protection;
            return true;
        }
        

        // ** IF THE USER WANTS TO HIJACK A THREAD, THAT IS THE USERS CHOICE NOT MINE **

        // -----------------------
        // STOP/PROT SECTION
        // -----------------------


        // _stop_proc <- stops a process
        void _stop_proc(uint32_t pid) // 4
        {
            if (ioctl(m_fd, NEUTRON_STOP_PROC, &pid) < 0)
            {
                throw std::runtime_error("NEUTRON_STOP_PROC failed");
            }
        }
        // _cont_proc <- contiunes a stopped process (_stop_proc)
        void _cont_proc(uint32_t pid) // 5
        {
            if (ioctl(m_fd, NEUTRON_CONT_PROC, &pid))
            {
                throw std::runtime_error("NEUTRON_CONT_PROC failed");
            }
        }

        // _allocate_mem <- allocates remote virtual memory in target process
        uint64_t _allocate_mem(uint32_t pid, uint64_t address, uint64_t size, uint32_t prot, uint32_t flags = 0)
        {
            neutron_vmem_context ctx;
            ctx.target_pid = pid;
            ctx.address = address;
            ctx.size = size;
            ctx.prot = prot;
            ctx.flags = flags;
            ctx.status = 0;

            if (ioctl(m_fd, NEUTRON_VIRTUAL_MEMD, &ctx) < 0)
            {
                throw std::runtime_error("NEUTRON_VIRTUAL_MEMD failed");
            }
            return ctx.address;
        }

        // _free_mem <- frees remote virtual memory in target process
        void _free_mem(uint32_t pid, uint64_t address, uint64_t size)
        {
            neutron_vmem_context ctx;
            ctx.target_pid = pid;
            ctx.address = address;
            ctx.size = size;
            ctx.prot = 0;
            ctx.flags = 0;
            ctx.status = 0;

            if (ioctl(m_fd, NEUTRON_VIRTUAL_MEMFR, &ctx) < 0)
            {
                throw std::runtime_error("NEUTRON_VIRTUAL_MEMFR failed");
            }
        }




    };
}

// ----------------------------------------------
// need to add _AOB_SCAN, _POKEHOLE, _QUERY_MEMORY, 
// _VIRTUAL_MEMD, _VIRTUAL_MEMFR, _CORE_INJECT, _CREATE_THREAD, _HIJACK_THREAD (8)
// ----------------------------------------------