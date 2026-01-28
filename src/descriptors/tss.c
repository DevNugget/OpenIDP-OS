#include <tss.h>

tss_t tss;

void tss_init(void) {
    memset(&tss, 0, sizeof(tss_t));
    // load tss entry to gdt
    // flush the tss register
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}