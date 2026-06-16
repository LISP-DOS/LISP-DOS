typedef unsigned long long uint64_t;

static inline void sys_write(const char *str) {
    __asm__ volatile (
        "mov $1, %%rax\n"     // syscall_num = 1 (write)
        "mov $1, %%rdi\n"     // arg1 = stdout (ignorado)
        "mov %0, %%rsi\n"     // arg2 = str
        "mov $0, %%rdx\n"     // arg3 = size (ignorado por enquanto)
        "syscall\n"
        : : "r"(str) : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
}

static inline void sys_exit(void) {
    __asm__ volatile (
        "mov $60, %%rax\n"    // syscall_num = 60 (exit)
        "syscall\n"
        : : : "rax", "rcx", "r11"
    );
}

void _start(void) {
    sys_write("Ola do Ring 3! (User Space)\n");
    sys_exit();
}
