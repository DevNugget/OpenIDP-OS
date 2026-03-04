#include <libidp/stdio.h>
#include <libidp/syscall.h>
#include <stdint.h>

#define PROCVIEW_MAX 128

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

static void print_process_on_cpu(const process_user_info_t* entry, uint64_t cpu) {
    printf("    process: ");
    if (entry->name[0] != '\0') {
        printf("%s", entry->name);
    } else {
        printf("(unnamed)");
    }

    printf(" [pid=");
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
        uint64_t tid = entry->running_tids[i];
        if (tid == 0) {
            continue;
        }

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

void main(int argc, char** argv) {
    stdio_arginit(&argc, argv);

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

    printf("\x1b[1;36mOpenIDP Process Viewer\x1b[0m\n");
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

    sys_exit(0);
}
