// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
* This file is part of the DZ09 project.
*
* Copyright (C) 2021 - 2019 AJScorp
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include "systemconfig.h"
#include "tlsf.h"
#include "memory.h"

static uint8_t MemoryPool[SYSMEMSIZE] __attribute__ ((aligned (8), section (".noinit")));

extern uintptr_t __stack_top, __stack_limit;
extern uintptr_t __int_stack_top, __int_stack_limit;

size_t InitializeMemoryPool(void)
{
    uint32_t iflags = __disable_interrupts();
    size_t   Result;

    destroy_memory_pool(MemoryPool);

    Result = init_memory_pool(SYSMEMSIZE, MemoryPool);
    __restore_interrupts(iflags);

    return Result;
}

void *malloc(size_t size)
{
    uint32_t iflags = __disable_interrupts();
    void     *Result = tlsf_malloc(size);

    __restore_interrupts(iflags);

    return Result;
}

void free(void *ptr)
{
    uint32_t iflags = __disable_interrupts();

    tlsf_free(ptr);

    __restore_interrupts(iflags);
}

void *realloc(void *ptr, size_t size)
{
    uint32_t iflags = __disable_interrupts();
    void     *Result = tlsf_realloc(ptr, size);

    __restore_interrupts(iflags);

    return Result;
}

void *calloc(size_t nelem, size_t elem_size)
{
    uint32_t iflags = __disable_interrupts();
    void     *Result = tlsf_calloc(nelem, elem_size);

    __restore_interrupts(iflags);

    return Result;
}

uint32_t GetSysMemoryAddress(void)
{
    return (uint32_t)&MemoryPool;
}

boolean IsDynamicMemory(void *Memory)
{
    return (((uintptr_t)Memory >= (uintptr_t)&MemoryPool[0]) &&
            ((uintptr_t)Memory <= (uintptr_t)&MemoryPool[SYSMEMSIZE - 1])) ? true : false;
}

boolean IsStackMemory(void *Memory)
{
    return ((((uintptr_t)Memory >= (uintptr_t)&__stack_limit) &&
             ((uintptr_t)Memory < (uintptr_t)&__stack_top)) ||
            (((uintptr_t)Memory >= (uintptr_t)&__int_stack_limit) &&
             ((uintptr_t)Memory < (uintptr_t)&__int_stack_top))) ? true : false;
}

size_t GetTotalUsedMemory(void)
{
    return get_used_size(MemoryPool);
}
