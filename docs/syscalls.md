# OpenIDP System Call Reference

**Process & Execution Management**
* [`SYS_YIELD` (0)](#sys_yield): Yields the CPU.
* [`SYS_EXIT` (2)](#sys_exit): Terminates the current process.
* [`SYS_GETPID` (12)](#sys_getpid): Retrieves the process ID.
* [`SYS_SPAWN` (13)](#sys_spawn): Spawns a new child process.
* [`SYS_WAIT` (14)](#sys_wait): Waits for a process to change state.
* [`SYS_KILL` (15)](#sys_kill): Kills a specific process tree.

**File System & I/O**
* [`SYS_FS_OPEN` (16)](#sys_open): Opens a file or directory.
* [`SYS_FS_READ` (17)](#sys_read): Reads from a file descriptor.
* [`SYS_FS_WRITE` (20)](#sys_write): Writes to a file descriptor.
* [`SYS_FS_CLOSE` (18)](#sys_close): Closes an open file descriptor.
* [`SYS_FS_READDIR` (24)](#sys_readdir): Reads a directory entry.
* [`SYS_GETCWD` (25)](#sys_getcwd): Gets the current working directory.
* [`SYS_CHDIR` (26)](#sys_chdir): Changes the current working directory.
* [`SYS_PIPE` (19)](#sys_pipe): Creates an inter-process communication pipe.
* [`SYS_DUP2` (23)](#sys_dup2): Duplicates a file descriptor.

**Shared Memory Management**
* [`SYS_SHM_CREATE` (3)](#sys_shm_create): Creates a new shared memory segment.
* [`SYS_SHM_MAP` (4)](#sys_shm_map): Maps a shared memory segment into the process space.
* [`SYS_SHM_UNMAP` (5)](#sys_shm_unmap): Unmaps a shared memory segment.
* [`SYS_SHM_DESTROY` (6)](#sys_shm_destroy): Destroys a shared memory segment.

**Hardware, Devices**
* [`SYS_FRAMEBUFFER_GET_INFO` (7)](#sys_framebuffer_get_info): Fetches framebuffer metadata.
* [`SYS_KEYBOARD_POLL` (8)](#sys_keyboard_poll): Checks if keyboard events are pending.
* [`SYS_KEYBOARD_READ` (9)](#sys_keyboard_read): Reads the next keyboard event.
* [`SYS_MOUSE_POLL` (10)](#sys_mouse_poll): Checks if mouse events are pending.
* [`SYS_MOUSE_READ` (11)](#sys_mouse_read): Reads the next mouse event.

**System Information & Debugging**
* [`SYS_PRINT` (1)](#sys_print): Directly prints a debug string via the kernel serial port.
* [`SYS_SYSINFO` (21)](#sys_info): Retrieves system hardware and uptime statistics.
* [`SYS_PROC_LIST` (22)](#sys_proc_list): Retrieves a snapshot of all running processes.

---

## Detailed System Call Reference

*Note: All userspace C wrappers documented below are defined in the `libidp` standard library. Success generally returns an integer `0` (or greater), while failure returns `-1` (`ERR_FAIL`).*

### Process & Execution Management

<a id="sys_yield"></a>
#### `int sys_yield(void)`
Explicitly yields the processor's remaining time slice, instructing the scheduler to execute the next thread in the queue. Returns `0` immediately when the thread resumes execution.

<a id="sys_exit"></a>
#### `void sys_exit(int code)`
Terminates the current process tree with the provided exit code. This system call halts the thread loop permanently using a continuous `pause` instruction in userspace so it never returns.

<a id="sys_getpid"></a>
#### `int sys_getpid(void)`
Retrieves the unique integer Process ID (PID) of the currently executing process.

<a id="sys_spawn"></a>
#### `int sys_spawn(const char* path, const char** argv)`
Spawns a new child process using the executable located at `path`, passing the arguments `argv`. Returns the new child's PID on success, or `-1` if the executable could not be loaded. 

<a id="sys_wait"></a>
#### `int sys_wait(int pid, int* out_exit_code)`
Blocks the current process until the child process specified by `pid` exits. The child's exit code is populated into the `out_exit_code` pointer.

<a id="sys_kill"></a>
#### `int sys_kill(int pid)`
Kills the specified process tree entirely, forcibly terminating it with exit code `137`.

### File System & I/O

<a id="sys_write"></a>
#### `int sys_write(uint64_t fd, const void* buffer, uint64_t bytes, uint64_t* out_written)`
Writes up to `bytes` of data from `buffer` to the file descriptor `fd`. The actual number of bytes written is returned in the `out_written` pointer. 

For standard I/O, `fd` is utilized as follows:
* `0`: Standard Input (stdin)
* `1`: Standard Output (stdout)
* `2`: Standard Error (stderr)

Application programs dynamically inherit these handles upon execution. For instance, the shell application dynamically appends its own stdio handles (which are routed through the terminal emulator interface) to the respective processes that it spawns. 

<a id="sys_read"></a>
#### `int sys_read(uint64_t fd, void* buffer, uint64_t bytes, uint64_t* out_read)`
Reads up to `bytes` of data from the file descriptor `fd` into `buffer`. The actual number of bytes read is populated in the `out_read` variable. Just like `sys_write`, standard input reads from `fd` `0`.

<a id="sys_open"></a>
#### `uint64_t sys_open(const char* path, uint32_t flags)`
Opens a file or directory located at `path`. Acceptable flags include:
* `IDP_O_RDONLY` (0x1)
* `IDP_O_WRONLY` (0x2)
* `IDP_O_CREATE` (0x4)
* `IDP_O_DIRECTORY` (0x8)

Returns a new file descriptor upon success. The kernel strictly allocates new file descriptors starting from index `3`, permanently reserving indices 0, 1, and 2 for standard IO.

<a id="sys_close"></a>
#### `int sys_close(uint64_t fd)`
Closes an active file descriptor, freeing it in the process's file descriptor table. Returns `0` on success or `-1` if the file descriptor was invalid.

<a id="sys_readdir"></a>
#### `int sys_readdir(uint64_t fd, idp_dirent_t* out_entry)`
Reads the next directory entry from an open directory descriptor `fd` and stores it in `out_entry`. The `idp_dirent_t` structure contains the file's name and its type (file vs directory).

<a id="sys_pipe"></a>
#### `int sys_pipe(uint64_t* read_fd, uint64_t* write_fd)`
Creates an anonymous pipe, storing the read handle in `read_fd` and the write handle in `write_fd`. Returns `0` on success.

<a id="sys_dup2"></a>
#### `int sys_dup2(uint64_t oldfd, uint64_t newfd)`
Duplicates the open descriptor `oldfd` into the descriptor slot `newfd`. If `newfd` was already open, it is safely closed first before the duplication occurs.

<a id="sys_getcwd"></a>
#### `int sys_getcwd(char* buf, size_t size)`
Copies the current process's absolute working directory string into `buf`, constrained to a maximum of `size` bytes. Returns `0` on success.

<a id="sys_chdir"></a>
#### `int sys_chdir(const char* path)`
Changes the process's current working directory to `path`. The kernel validates the path by temporarily attempting to open it as a directory before updating the internal working directory buffer.

### Shared Memory Management

<a id="sys_shm_create"></a>
#### `int64_t sys_shm_create(uint64_t size_bytes)`
Requests the kernel to create a block of shared memory sized at `size_bytes`. Returns a globally unique 64-bit integer handle to refer to this region on success, or `-1` on failure.

<a id="sys_shm_map"></a>
#### `void* sys_shm_map(uint64_t handle)`
Maps the shared memory segment identified by `handle` into the current process's virtual memory layout. Returns a pointer to the newly mapped address.

<a id="sys_shm_unmap"></a>
#### `int sys_shm_unmap(void* address)`
Unmaps the shared memory block currently residing at the virtual `address` from the process's page tables.

<a id="sys_shm_destroy"></a>
#### `int sys_shm_destroy(uint64_t handle)`
Destroys the shared memory object associated with `handle`, cleaning up underlying physical allocations once all processes unmap it.

### Hardware, Devices & UI

<a id="sys_framebuffer_get_info"></a>
#### `int sys_framebuffer_get_info(framebuffer_user_info_t* out_info)`
Retrieves metadata about the graphical framebuffer, storing it in `out_info`. This structure contains the screen width, height, pitch, bits-per-pixel, byte size, and the shared memory handle required to map the framebuffer directly into userspace.

<a id="sys_keyboard_poll"></a>
#### `int sys_keyboard_poll(void)`
Checks if there are any pending keyboard events in the system's input ring buffer. Returns `1` if an event is available to read, and `0` otherwise.

<a id="sys_keyboard_read"></a>
#### `int sys_keyboard_read(key_event_t* out_event)`
Consumes the next available keyboard event from the queue and stores it into `out_event`. The `key_event_t` contains a numeric keycode, the pressed status, and a bitmask of modifier keys (such as `CTRL`, `ALT`, `SHIFT`).

<a id="sys_mouse_poll"></a>
#### `int sys_mouse_poll(void)`
Checks if there are pending mouse events available. Returns `1` if events are waiting, and `0` if empty.

<a id="sys_mouse_read"></a>
#### `int sys_mouse_read(mouse_event_user_t* out_event)`
Reads the most recent mouse event into `out_event`, providing the `delta_x`, `delta_y` offset changes, and a bitmask representing the current state of the mouse buttons.

### System Information & Debugging

<a id="sys_print"></a>
#### `int sys_print(const char* str)`
Directly writes a string `str` to the kernel's serial debug interface via port `COM1`. The output is always prefixed with the tag `[USER] `.

<a id="sys_info"></a>
#### `int sys_info(sysinfo_t* info)`
Fills the `sysinfo_t` struct with system-wide metadata including:
* `uptime_ms`: System uptime in milliseconds.
* `total_ram`: Total installed physical memory.
* `free_ram`: Currently available physical memory.
* `procs`: Currently active process count.
* `cpus`: Total active symmetric multiprocessing (SMP) CPU cores.

<a id="sys_proc_list"></a>
#### `int sys_proc_list(process_user_info_t* out_entries, uint64_t capacity, uint64_t* out_count)`
Populates the `out_entries` array with detailed runtime snapshots of up to `capacity` processes. Each entry contains the PID, parent PID, thread count, running CPUs mask, and the name of the process (up to 64 characters). The total number of valid entries copied is returned via the `out_count` pointer.