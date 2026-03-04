#include <libidp/stdio.h>
#include <libidp/syscall.h>
#include <stdint.h>

#define PROCVIEW_MAX 128

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static void print_u64(uint64_t value) {
    char buffer[32];
    int index = 0;

    if (value == 0) {
        putchar('0');
        return;
    }

    while (value > 0 && index < (int)sizeof(buffer)) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        putchar(buffer[--index]);
    }
}

static void print_u64_padded(uint64_t value, int width) {
    char buffer[32];
    int index = 0;

    if (value == 0) {
        buffer[index++] = '0';
    } else {
        uint64_t temp = value;
        while (temp > 0 && index < (int)sizeof(buffer)) {
            buffer[index++] = (char)('0' + (temp % 10));
            temp /= 10;
        }
    }

    int printed = index;
    while (index > 0) {
        putchar(buffer[--index]);
    }
    while (printed < width) {
        putchar(' ');
        printed++;
    }
}

static void print_process_on_cpu(const process_user_info_t* entry, uint64_t cpu) {
    printf("    process: %s [pid=", entry->name[0] ? entry->name : "(unnamed)");
    print_u64(entry->pid);
    printf(", ppid=");
    print_u64(entry->parent_pid);
    printf(", threads=");
    print_u64(entry->thread_count);
    printf(", running=");
    print_u64(entry->running_thread_count);
    printf("]\n");

    int printed_thread = 0;
    for (uint32_t i = 0; i < entry->running_tid_count; ++i) {
        if (entry->running_cpus[i] != cpu) {
            continue;
        }

        uint64_t tid = entry->running_tids[i];
        if (tid == 0) continue;

        printf("        - running thread tid=");
        print_u64(tid);
        printf(" on cpu ");
        print_u64(cpu);
        printf("\n");
        printed_thread = 1;
    }

    if (!printed_thread) {
        printf("        - running thread IDs unavailable\n");
    }
}

static void sleep_ms(uint64_t ms) {
    sysinfo_t info;
    if (sys_info(&info) != ERR_SUCCESS) return;
    
    uint64_t start_time = info.uptime_ms;
    
    while (1) {
        if (sys_info(&info) != ERR_SUCCESS) break;
        if ((info.uptime_ms - start_time) >= ms) break;
        sys_yield();
    }
}

static void print_snapshot() {
    process_user_info_t entries[PROCVIEW_MAX];
    uint64_t total = 0;

    if (sys_proc_list(entries, PROCVIEW_MAX, &total) != ERR_SUCCESS) {
        puts("procview: unable to read process list");
        sys_exit(1);
    }

    sysinfo_t info;
    uint64_t cpu_count = 1;
    if (sys_info(&info) == ERR_SUCCESS && info.cpus > 0) {
        cpu_count = info.cpus;
    }

    uint64_t shown = total;
    if (shown > PROCVIEW_MAX) {
        shown = PROCVIEW_MAX;
    }

    printf("\x1b[1;36mProcess Viewer (Snaphot)\x1b[0m\n");
    printf("CPUs: ");
    print_u64(cpu_count);
    printf(" | active processes shown: ");
    print_u64(shown);
    printf(" of ");
    print_u64(total);
    printf("\n\n");

    for (uint64_t cpu = 0; cpu < cpu_count; ++cpu) {
        printf("CPU ");
        print_u64(cpu);
        printf(":\n");

        int printed_any = 0;
        uint64_t cpu_bit = (cpu < 64) ? (1ULL << cpu) : 0;

        for (uint64_t i = 0; i < shown; ++i) {
            if (cpu_bit == 0 || (entries[i].cpu_mask & cpu_bit) == 0) {
                continue;
            }

            print_process_on_cpu(&entries[i], cpu);
            printed_any = 1;
        }

        if (!printed_any) {
            printf("    (no running processes)\n");
        }

        putchar('\n');
    }

    printf("Not currently running on a CPU:\n");
    int printed_unassigned = 0;
    for (uint64_t i = 0; i < shown; ++i) {
        if (entries[i].cpu_mask != 0) {
            continue;
        }

        printf("    process: %s [pid=", entries[i].name[0] ? entries[i].name : "(unnamed)");
        print_u64(entries[i].pid);
        printf(", threads=");
        print_u64(entries[i].thread_count);
        printf("]\n");
        printed_unassigned = 1;
    }
    if (!printed_unassigned) {
        printf("    (none)\n");
    }

    if (total > PROCVIEW_MAX) {
        printf("\nprocview: output truncated to ");
        print_u64(PROCVIEW_MAX);
        printf(" entries\n");
    }
}

static void print_table_loop() {
    printf("\x1b[?25l");

    while (1) {
        process_user_info_t entries[PROCVIEW_MAX];
        uint64_t total = 0;

        if (sys_proc_list(entries, PROCVIEW_MAX, &total) != ERR_SUCCESS) {
            printf("\x1b[?25h");
            puts("procview: unable to read process list\n");
            sys_exit(1);
        }

        sysinfo_t info;
        uint64_t cpu_count = 1;
        if (sys_info(&info) == ERR_SUCCESS && info.cpus > 0) {
            cpu_count = info.cpus;
        }

        uint64_t shown = total;
        if (shown > PROCVIEW_MAX) {
            shown = PROCVIEW_MAX;
        }

        printf("\x1b[H");

        printf("\x1b[1;36m\x1b[KProcess Viewer (Looping)\x1b[0m\n");
        printf("CPUs: ");
        print_u64(cpu_count);
        printf(" | Procs: ");
        print_u64(shown);
        printf("/");
        print_u64(total);
        printf(" | Uptime: ");
        uint64_t total_seconds = info.uptime_ms / 1000;
        uint64_t hours = total_seconds / 3600;
        uint64_t minutes = (total_seconds % 3600) / 60;
        uint64_t seconds = total_seconds % 60;

        print_u64(hours);
        printf("h ");
        print_u64(minutes);
        printf("m ");
        print_u64(seconds);
        printf("s\x1b[K\n\x1b[K\n");

        printf("\x1b[1;37m\x1b[35mPID     PPID    THREADS RUNNING CPU     NAME\x1b[0m\x1b[K\n");
        printf("----------------------------------------------------------\x1b[K\n");

        for (uint64_t i = 0; i < shown; ++i) {
            print_u64_padded(entries[i].pid, 8);
            print_u64_padded(entries[i].parent_pid, 8);
            print_u64_padded(entries[i].thread_count, 8);
            if (entries[i].running_thread_count == 0) {
                printf("\x1b[31m");
            } else {
                printf("\x1b[32m");
            }
            print_u64_padded(entries[i].running_thread_count, 8);
            printf("\x1b[0m");

            if (entries[i].cpu_mask == 0) {
                printf("\x1b[31m-\x1b[0m       ");
            } else {
                uint64_t first_cpu = 0;
                for (int c = 0; c < 64; c++) {
                    if (entries[i].cpu_mask & (1ULL << c)) {
                        first_cpu = c;
                        break;
                    }
                }
                printf("\x1b[97m");
                print_u64_padded(first_cpu, 8);
                printf("\x1b[0m");
            }

            if (entries[i].cpu_mask == 0) {
                printf("\x1b[90m%s\x1b[0m\x1b[K\n", entries[i].name[0] ? entries[i].name : "(unnamed)");
            } else {
                printf("%s\x1b[K\n", entries[i].name[0] ? entries[i].name : "(unnamed)");
            }
        }

        if (total > PROCVIEW_MAX) {
            printf("\nprocview: output truncated to ");
            print_u64(PROCVIEW_MAX);
            printf(" entries\x1b[K\n");
        }

        printf("\x1b[J");

        sleep_ms(1000);
    }
}

void main(int argc, char** argv) {
    stdio_arginit(&argc, argv);

    printf("\x1b]0;lsproc\x07");

    int loop_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "-loop")) {
            loop_mode = 1;
        } else if (streq(argv[i], "-snap")) {
            loop_mode = 0;
        }
    }

    if (loop_mode) {
        print_table_loop();
    } else {
        print_snapshot();
    }

    sys_exit(0);
}