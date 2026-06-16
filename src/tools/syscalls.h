#pragma once

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned long long size_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

static inline uint64_t syscall0(uint64_t num) {
    uint64_t ret;
    __asm__ volatile (
        "syscall\n"
        : "=a"(ret) : "a"(num) : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall1(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile (
        "syscall\n"
        : "=a"(ret) : "a"(num), "D"(arg1) : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    __asm__ volatile (
        "syscall\n"
        : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    __asm__ volatile (
        "syscall\n"
        : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3) : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void sys_write(const char *str) {
    syscall3(1, 1, (uint64_t)str, 0);
}

static inline void sys_clear(void) {
    syscall0(2);
}

static inline void sys_set_cursor(int row, int col) {
    syscall2(3, row, col);
}

static inline void sys_putchar(char c) {
    syscall1(4, c);
}

static inline void sys_set_color(uint32_t bg, uint32_t fg) {
    syscall2(5, bg, fg);
}

static inline int sys_get_max_rows(void) {
    return (int)syscall0(6);
}

static inline char sys_keyboard_getchar(void) {
    return (char)syscall0(7);
}

static inline int sys_fat16_read_file(const char* filename, uint8_t* buffer) {
    return (int)syscall2(8, (uint64_t)filename, (uint64_t)buffer);
}

static inline int sys_fat16_write_file(const char* filename, const uint8_t* buffer, uint32_t size) {
    return (int)syscall3(9, (uint64_t)filename, (uint64_t)buffer, size);
}

static inline int sys_fat16_get_file_size(const char* filename) {
    return (int)syscall1(10, (uint64_t)filename);
}

static inline void sys_exit(void) {
    syscall0(60);
}

#define vga_clear sys_clear
#define vga_set_cursor sys_set_cursor
#define vga_print sys_write
#define vga_putchar sys_putchar
#define vga_set_color sys_set_color
#define vga_get_max_rows sys_get_max_rows
#define keyboard_getchar sys_keyboard_getchar
#define fat16_read_file sys_fat16_read_file
#define fat16_write_file sys_fat16_write_file
#define fat16_get_file_size sys_fat16_get_file_size
