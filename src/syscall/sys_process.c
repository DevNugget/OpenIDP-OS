#include <ksyscall.h>

extern task_t* current_task;

/* Process creation/exit */

int sys_exec(const char* path, int argc, char** argv) {
    return create_user_process_from_file(path, argc, argv, 0);
}

void sys_exit(int code) {
    serial_printf("[KERNEL] Process exited with code %d\n", code);
    
    task_exit();

    while(1) {
        serial_printf("Syscall exit error, should not reach here! HANGING\n");
        asm volatile("hlt");
    }
}

/* Inter-process communication */

int sys_ipc_send(int dest_pid, int type, uint64_t d1, uint64_t d2, uint64_t d3) {
    task_t* target = get_task_by_pid(dest_pid);
    if (!target) return -1;

    if (target->msg_count >= MSG_QUEUE_SIZE) return -2; // Queue full

    message_t* msg = &target->msgs[target->msg_tail];
    msg->sender_pid = current_task->pid;
    msg->type = type;
    msg->data1 = d1;
    msg->data2 = d2;
    msg->data3 = d3;

    target->msg_tail = (target->msg_tail + 1) % MSG_QUEUE_SIZE;
    target->msg_count++;
    
    return 0;
}

int sys_ipc_recv(message_t* out_msg) {
    if (current_task->msg_count == 0) return -1;

    *out_msg = current_task->msgs[current_task->msg_head];
    current_task->msg_head = (current_task->msg_head + 1) % MSG_QUEUE_SIZE;
    current_task->msg_count--;

    return 0;
}