#include <drivers/framebuffer.h>

#include <drivers/com1.h>
#include <memory/shm.h>
#include <utility/hhdm.h>
#include <utility/kstring.h>
#include <limine.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static framebuffer_user_info_t g_fb_info;
static int g_fb_ready = 0;

int framebuffer_init_shared_memory(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        serial_write_str("[FRAMEBUFFER] No framebuffer response from Limine\n");
        return -1;
    }

    struct limine_framebuffer* framebuffer = framebuffer_request.response->framebuffers[0];
    if (framebuffer == NULL || framebuffer->address == NULL) {
        serial_write_str("[FRAMEBUFFER] Invalid framebuffer descriptor\n");
        return -1;
    }

    size_t size_bytes = framebuffer->pitch * framebuffer->height;
    uint64_t shm_handle = 0;

    if (shm_create_from_phys(virt_to_phys((virt_addr_t)framebuffer->address), size_bytes, &shm_handle) != 0) {
        serial_write_str("[FRAMEBUFFER] Failed to create framebuffer shared-memory segment\n");
        return -1;
    }

    memset(&g_fb_info, 0, sizeof(g_fb_info));
    g_fb_info.width = framebuffer->width;
    g_fb_info.height = framebuffer->height;
    g_fb_info.pitch = framebuffer->pitch;
    g_fb_info.bpp = framebuffer->bpp;
    g_fb_info.size_bytes = size_bytes;
    g_fb_info.shm_handle = shm_handle;

    g_fb_ready = 1;
    serial_printf("[FRAMEBUFFER] Shared framebuffer ready: %ux%u pitch=%u bpp=%u handle=%u\n",
                  (uint32_t)g_fb_info.width,
                  (uint32_t)g_fb_info.height,
                  (uint32_t)g_fb_info.pitch,
                  (uint32_t)g_fb_info.bpp,
                  (uint32_t)g_fb_info.shm_handle);

    return 0;
}

int framebuffer_get_user_info(framebuffer_user_info_t* out_info) {
    if (!g_fb_ready || out_info == NULL) {
        return -1;
    }

    *out_info = g_fb_info;
    return 0;
}
