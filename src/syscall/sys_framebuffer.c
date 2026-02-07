#include <ksyscall.h>

extern task_t* current_task;

int sys_get_fb_info(struct fb_info* user_out) {
    task_t* t = current_task;

    if (!t->is_wm) {
        return -1;
    }

    struct fb_info info;
    info.fb_addr = USER_FB_BASE;
    info.fb_width = fb_width();
    info.fb_height = fb_height();
    info.fb_pitch = fb_pitch();
    info.fb_bpp = fb_bpp();

    memcpy(user_out, &info, sizeof(info));
    return 0;
}