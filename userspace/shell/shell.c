#include "../libc/stdio.h"
#include "../libc/string.h"
#include "../openidp.h"

#define MAX_CMD_LEN 256
#define MAX_PATH_LEN 256

char current_directory[MAX_PATH_LEN] = "/";

int resolve_path(char* target_buf, const char* base, const char* input);

// Helper to read a line from our new getchar()
int shell_readline(char* buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = getchar(); // Blocks waiting for Terminal
        
        if (c == EOF) {
            return EOF; // Pass up the exit signal
        }

        if (c == '\n') {
            putchar('\n');
            buf[i] = 0;
            return 0;
        } 
        else if (c == '\b') {
            if (i > 0) {
                i--;
                // Visual backspace handling
                // We send backspace -> space -> backspace to erase on screen
                putchar('\b'); putchar(' '); putchar('\b');
            }
        } 
        else {
            buf[i++] = c;
            putchar(c); // Local Echo
        }
    }
    buf[i] = 0;
    return 0;
}

void _start() {
    // 1. Wait for connection from Terminal
    stdio_init();

    printf("\033[37m--> idpshell \033[36m(isolated binary)\n\033[37m");

    char cmd_buffer[MAX_CMD_LEN];
    char temp_path[MAX_PATH_LEN];

    while (1) {
        printf("\033[35m@\033[36m[\033[34m%s\033[36m] :: \033[37m", current_directory);
        
        if (shell_readline(cmd_buffer, MAX_CMD_LEN) == EOF) {
            break; 
        }

        if (cmd_buffer[0] != 0) {
            // 1. Tokenize (Your existing code)
            char* argv[16];
            argv[0] = current_directory; 
            int argc = 1;
            char* token = strtok(cmd_buffer, " ");
            while (token != NULL && argc < 16) {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
            argv[argc] = NULL; 

            // Safety check: Did we actually get a command token?
            if (argc < 2) continue; 

            char* command = argv[1];      // The command (e.g., "cd", "ls")
            char* argument = argv[2];     // The first arg (e.g., "/path")

            // 2. Dispatch using tokens instead of raw buffer
            if (strcmp(command, "help") == 0) {
                printf("Commands: help, clear, echo, cd\n");
            }
            else if (strcmp(command, "cd") == 0) {
                // Handle "cd" (go to root)
                if (argument == NULL) {
                     strcpy(current_directory, "/");
                }
                // Handle "cd <path>"
                else {
                    resolve_path(temp_path, current_directory, argument);

                    if (strcmp(temp_path, "/") == 0) {
                        strcpy(current_directory, "/");
                    }
                    else if (strlen(temp_path) < MAX_PATH_LEN) {
                        struct kstat st;
                        if (sys_stat(temp_path, &st) == 0) {
                            if (st.flags == 1) { // It is a directory
                                strcpy(current_directory, temp_path);
                            } else {
                                printf("\033[36m[\033[37midpshell-cd\033[36m] :: \033[33m%s\033[31m not a directory\033[37m\n", temp_path);
                            }
                        } else {
                            printf("\033[36m[\033[37midpshell-cd\033[36m] :: \033[33m%s\033[31m not found\033[37m\n", temp_path);
                        }
                    } else {
                        printf("\033[36m[\033[37midpshell-cd\033[36m] :: \033[31mPath too long\033[37m\n");
                    }
                }
            }
            else if (strcmp(command, "clear") == 0) {
                clear_screen();
            }
            else {
                // External Command Execution
                // Note: We use 'command' (argv[1]) as the executable path
                if (command[0] == '/') {
                    // Absolute path
                    int pid = sys_exec(argv[1], argc, argv);
                    if (pid < 0) printf("\033[31mCommand failed:\033[37m %s \033[36m(invalid command or path to elf)\033[37m\n", command);
                    extern int terminal_pid;
                    sys_ipc_send(pid, MSG_HANDSHAKE, terminal_pid, 0, 0);
                } else {
                    // Relative path
                    resolve_path(temp_path, current_directory, command);
                    int pid = sys_exec(temp_path, argc, argv); // Execute resolved path
                    if (pid < 0) printf("\033[31mCommand failed:\033[37m %s \033[36m(invalid command or path to elf)\033[37m\n", command);
                    extern int terminal_pid;
                    sys_ipc_send(pid, MSG_HANDSHAKE, terminal_pid, 0, 0);
                }
            }
        }
    }
    sys_exit(0);
}

int resolve_path(char* target_buf, const char* base, const char* input) {
    char temp[256]; // Working buffer to avoid modifying inputs
    char* tokens[64]; // Pointers to path segments
    int depth = 0;    // Current depth (stack pointer)
    
    // Determine starting point
    if (input[0] == '/') {
        // Absolute path -> Start fresh
        strcpy(temp, input);
    } else {
        // Relative path -> Start with base, append input
        strcpy(temp, base);
        int len = strlen(temp);
        if (len > 0 && temp[len-1] != '/') {
            strcat(temp, "/");
        }
        strcat(temp, input);
    }

    // Tokenize the full string (split by '/')
    char* token = strtok(temp, "/");
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Ignore current directory marker
        } 
        else if (strcmp(token, "..") == 0) {
            // Go back one level
            if (depth > 0) {
                depth--;
            }
            // If depth is already 0, we stay at root 
        } 
        else if (strlen(token) > 0) {
            // Push regular directory name
            tokens[depth++] = token;
        }
        
        token = strtok(NULL, "/");
    }

    // Reconstruct the path
    if (depth == 0) {
        // Special case: We are at root
        strcpy(target_buf, "/");
        return 0;
    }

    // Build the string: "/dir1/dir2"
    target_buf[0] = 0; // Initialize empty
    for (int i = 0; i < depth; i++) {
        strcat(target_buf, "/");
        strcat(target_buf, tokens[i]);
    }

    return 0;
}
