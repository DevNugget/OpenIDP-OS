#include "../openidp.h"
#include "../libc/stdio.h"
#include "../libc/string.h"

// The Kernel puts argc in RDI and argv in RSI (Standard System V ABI).
// Defining _start with these arguments makes the compiler read the registers correctly.
void _start(int argc, char** argv) {
    stdio_init();
    
    char* target_path = "/"; // Fallback default
    char path_buffer[256];   // Buffer to build absolute path

    // argv[0] = Current Working Directory (Absolute)
    // argv[1] = Program Name ("ls")
    // argv[2] = Argument (Optional, e.g., "docs")

    if (argc > 2) {
        char* user_arg = argv[2];

        // If the user provided a relative path prepend CWD
        if (user_arg[0] != '/') {
            strcpy(path_buffer, argv[0]);
            
            // Ensure CWD ends with a slash before appending
            int len = strlen(path_buffer);
            if (len > 0 && path_buffer[len-1] != '/') {
                strcat(path_buffer, "/");
            }
            strcat(path_buffer, user_arg);
            target_path = path_buffer;
        } else {
            // User provided absolute path
            target_path = user_arg;
        }
    } 
    else if (argc >= 1) {
        // Default to the Current Working Directory provided by shell
        target_path = argv[0];
    }

    struct kdirent entry;
    int index = 0;

    printf("\n\033[36m--- %s ---\033[37m\n", target_path);

    while (1) {
        int res = sys_dir_read(target_path, index, &entry);

        if (res == -2) break; // End of Directory
        if (res < 0) {
            printf("\033[31mError: Directory not found\033[37m\n");
            break;
        }

        str_tolower(entry.name);
        if (entry.is_dir) {
            printf("\033[34m%s/\n\033[37m", entry.name);
        } else {
            printf("%s\n", entry.name);
        }
        
        index++;
    }
    printf("\033[36m--- press enter ---\033[37m\n");

    sys_exit(0);
}