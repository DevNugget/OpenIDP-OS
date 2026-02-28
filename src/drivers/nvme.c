#include <drivers/nvme.h>

#include <drivers/com1.h>
#include <drivers/pci.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <utility/hhdm.h>
#include <utility/align.h>
#include <utility/kstring.h>

#define NVME_PCI_CLASS     0x01
#define NVME_PCI_SUBCLASS  0x08
#define NVME_PCI_PROGIF    0x02

#define NVME_REG_CAP       0x0000
#define NVME_REG_VS        0x0008
#define NVME_REG_CC        0x0014
#define NVME_REG_CSTS      0x001C
#define NVME_REG_AQA       0x0024
#define NVME_REG_ASQ       0x0028
#define NVME_REG_ACQ       0x0030
#define NVME_REG_DBS       0x1000

#define NVME_CC_EN         (1U << 0)
#define NVME_CSTS_RDY      (1U << 0)

#define NVME_ADMIN_OP_CREATE_IO_SQ 0x01
#define NVME_ADMIN_OP_CREATE_IO_CQ 0x05
#define NVME_ADMIN_OP_IDENTIFY     0x06
#define NVME_ADMIN_OP_SET_FEATURES 0x09

#define NVME_IDENTIFY_CNS_NAMESPACE      0x00
#define NVME_IDENTIFY_CNS_CONTROLLER     0x01
#define NVME_IDENTIFY_CNS_ACTIVE_NS_LIST 0x02

#define NVME_NVM_OP_WRITE          0x01
#define NVME_NVM_OP_READ           0x02

#define NVME_FEAT_NUM_QUEUES       0x07

#define NVME_ADMIN_Q_DEPTH         64
#define NVME_IO_Q_DEPTH            64

#define NVME_ADMIN_QID             0
#define NVME_IO_QID                1

#define NVME_MMIO_MAP_SIZE  (64 * 1024)

typedef struct __attribute__((packed)) {
    uint8_t opcode;
    uint8_t flags;
    uint16_t command_id;
    uint32_t nsid;
    uint64_t rsvd2;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_sqe_t;

typedef struct __attribute__((packed)) {
    uint32_t dw0;
    uint32_t rsvd;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t command_id;
    uint16_t status;
} nvme_cqe_t;

typedef struct {
    uint16_t qid;
    uint16_t depth;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint8_t cq_phase;

    nvme_sqe_t* sq;
    nvme_cqe_t* cq;
    uint64_t sq_phys;
    uint64_t cq_phys;
} nvme_queue_t;

typedef struct {
    bool initialized;
    volatile uint8_t* mmio;
    pci_device_info_t pci;

    nvme_namespace_t namespace;
    uint32_t doorbell_stride_bytes;

    nvme_queue_t admin_q;
    nvme_queue_t io_q;

    uint16_t next_cid;

    void* bounce;
    uint64_t bounce_phys;
} nvme_controller_t;

static nvme_controller_t g_nvme = {0};

static phys_addr_t* nvme_current_pml4(void) {
    phys_addr_t cr3 = read_cr3() & CR3_ADDR_MASK;
    return (phys_addr_t*)phys_to_virt(cr3);
}

static void nvme_map_page_current(phys_addr_t phys, uint64_t flags) {
    phys_addr_t* pml4 = nvme_current_pml4();
    virt_addr_t virt = (virt_addr_t)phys_to_virt(phys);
    vmm_map_page(pml4, virt, phys, flags);
}

static void nvme_map_mmio_range(phys_addr_t base, size_t bytes) {
    phys_addr_t start = ALIGN_DOWN(base, PAGE_SIZE);
    phys_addr_t end = ALIGN_UP(base + bytes, PAGE_SIZE);

    for (phys_addr_t p = start; p < end; p += PAGE_SIZE) {
        nvme_map_page_current(p, PT_FLAG_WRITE | PT_FLAG_PCD | PT_FLAG_PWT);
    }
}

static inline uint32_t nvme_reg_read32(uint32_t offset) {
    return *(volatile uint32_t*)(g_nvme.mmio + offset);
}

static inline uint64_t nvme_reg_read64(uint32_t offset) {
    uint64_t low = *(volatile uint32_t*)(g_nvme.mmio + offset);
    uint64_t high = *(volatile uint32_t*)(g_nvme.mmio + offset + 4);
    return low | (high << 32);
}

static inline void nvme_reg_write32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_nvme.mmio + offset) = value;
}

static inline void nvme_reg_write64(uint32_t offset, uint64_t value) {
    *(volatile uint32_t*)(g_nvme.mmio + offset) = (uint32_t)(value & 0xFFFFFFFFU);
    *(volatile uint32_t*)(g_nvme.mmio + offset + 4) = (uint32_t)(value >> 32);
}

static inline volatile uint32_t* nvme_db_sq(uint16_t qid) {
    uint32_t stride = g_nvme.doorbell_stride_bytes;
    return (volatile uint32_t*)(g_nvme.mmio + NVME_REG_DBS + ((uint32_t)(2U * qid) * stride));
}

static inline volatile uint32_t* nvme_db_cq(uint16_t qid) {
    uint32_t stride = g_nvme.doorbell_stride_bytes;
    return (volatile uint32_t*)(g_nvme.mmio + NVME_REG_DBS + ((uint32_t)((2U * qid) + 1U) * stride));
}

static void nvme_ring_sq_doorbell(const nvme_queue_t* q) {
    *nvme_db_sq(q->qid) = q->sq_tail;
}

static void nvme_ring_cq_doorbell(const nvme_queue_t* q) {
    *nvme_db_cq(q->qid) = q->cq_head;
}

static bool nvme_wait_ready(bool expected) {
    for (uint32_t i = 0; i < 20000000; i++) {
        bool ready = (nvme_reg_read32(NVME_REG_CSTS) & NVME_CSTS_RDY) != 0;
        if (ready == expected) {
            return true;
        }
    }

    return false;
}

static void* nvme_alloc_page(uint64_t* out_phys) {
    phys_addr_t phys = pmm_alloc(1);
    if (!phys) {
        return NULL;
    }

    nvme_map_page_current(phys, PT_FLAG_WRITE);
    void* virt = phys_to_virt(phys);
    memset(virt, 0, PAGE_SIZE);

    if (out_phys) {
        *out_phys = phys;
    }

    return virt;
}

static uint16_t nvme_next_cid(void) {
    g_nvme.next_cid++;
    if (g_nvme.next_cid == 0) {
        g_nvme.next_cid = 1;
    }
    return g_nvme.next_cid;
}

static bool nvme_submit_and_wait(nvme_queue_t* q, const nvme_sqe_t* cmd, nvme_cqe_t* out_cqe) {
    uint16_t cid = cmd->command_id;
    q->sq[q->sq_tail] = *cmd;
    q->sq_tail = (uint16_t)((q->sq_tail + 1) % q->depth);
    nvme_ring_sq_doorbell(q);

    for (uint32_t spin = 0; spin < 50000000; spin++) {
        nvme_cqe_t* cqe = &q->cq[q->cq_head];
        uint8_t phase = (uint8_t)(cqe->status & 0x1);

        if (phase != q->cq_phase) {
            continue;
        }

        nvme_cqe_t local = *cqe;

        q->cq_head = (uint16_t)((q->cq_head + 1) % q->depth);
        if (q->cq_head == 0) {
            q->cq_phase ^= 1;
        }
        nvme_ring_cq_doorbell(q);

        if (local.command_id != cid) {
            continue;
        }

        uint16_t status_code_type = (local.status >> 1) & 0x7FF;
        if (status_code_type != 0) {
            serial_printf("[NVME] CQE error qid=%u cid=%u status=0x%x sqid=%u\n",
                          q->qid,
                          cid,
                          status_code_type,
                          local.sq_id);
            return false;
        }

        if (out_cqe) {
            *out_cqe = local;
        }

        return true;
    }

    serial_printf("[NVME] Timeout waiting completion qid=%u cid=%u\n", q->qid, cid);
    return false;
}

static bool nvme_admin_identify(uint32_t nsid, uint32_t cns, uint64_t data_phys) {
    nvme_sqe_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_ADMIN_OP_IDENTIFY;
    cmd.command_id = nvme_next_cid();
    cmd.nsid = nsid;
    cmd.prp1 = data_phys;
    cmd.cdw10 = cns;

    return nvme_submit_and_wait(&g_nvme.admin_q, &cmd, NULL);
}

static bool nvme_admin_set_features_num_queues(uint16_t requested_io_queues) {
    nvme_sqe_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_ADMIN_OP_SET_FEATURES;
    cmd.command_id = nvme_next_cid();
    cmd.cdw10 = NVME_FEAT_NUM_QUEUES;

    uint32_t q = (uint32_t)(requested_io_queues - 1);
    cmd.cdw11 = (q << 16) | q;

    return nvme_submit_and_wait(&g_nvme.admin_q, &cmd, NULL);
}

static bool nvme_admin_create_io_cq(nvme_queue_t* q) {
    nvme_sqe_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_ADMIN_OP_CREATE_IO_CQ;
    cmd.command_id = nvme_next_cid();
    cmd.prp1 = q->cq_phys;
    cmd.cdw10 = ((uint32_t)(q->depth - 1) << 16) | q->qid;
    cmd.cdw11 = 0x1; // physically contiguous, polling mode (IEN=0)

    return nvme_submit_and_wait(&g_nvme.admin_q, &cmd, NULL);
}

static bool nvme_admin_create_io_sq(nvme_queue_t* q) {
    nvme_sqe_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_ADMIN_OP_CREATE_IO_SQ;
    cmd.command_id = nvme_next_cid();
    cmd.prp1 = q->sq_phys;
    cmd.cdw10 = ((uint32_t)(q->depth - 1) << 16) | q->qid;
    cmd.cdw11 = 0x1; // physically contiguous
    cmd.cdw11 |= ((uint32_t)q->qid << 16); // CQID to pair with

    return nvme_submit_and_wait(&g_nvme.admin_q, &cmd, NULL);
}

static bool nvme_controller_enable(void) {
    uint32_t cc = nvme_reg_read32(NVME_REG_CC);
    cc &= ~NVME_CC_EN;
    nvme_reg_write32(NVME_REG_CC, cc);

    if (!nvme_wait_ready(false)) {
        serial_write_str("[NVME] Controller did not clear RDY\n");
        return false;
    }

    g_nvme.admin_q.qid = NVME_ADMIN_QID;
    g_nvme.admin_q.depth = NVME_ADMIN_Q_DEPTH;
    g_nvme.admin_q.sq_tail = 0;
    g_nvme.admin_q.cq_head = 0;
    g_nvme.admin_q.cq_phase = 1;
    g_nvme.admin_q.sq = nvme_alloc_page(&g_nvme.admin_q.sq_phys);
    g_nvme.admin_q.cq = nvme_alloc_page(&g_nvme.admin_q.cq_phys);

    if (!g_nvme.admin_q.sq || !g_nvme.admin_q.cq) {
        serial_write_str("[NVME] Failed to allocate admin queues\n");
        return false;
    }

    nvme_reg_write32(NVME_REG_AQA,
                     ((uint32_t)(g_nvme.admin_q.depth - 1) << 16)
                     | (uint32_t)(g_nvme.admin_q.depth - 1));
    nvme_reg_write64(NVME_REG_ASQ, g_nvme.admin_q.sq_phys);
    nvme_reg_write64(NVME_REG_ACQ, g_nvme.admin_q.cq_phys);

    cc = 0;
    cc |= NVME_CC_EN;
    cc |= (6U << 16); // IOSQES = 2^6 = 64 bytes
    cc |= (4U << 20); // IOCQES = 2^4 = 16 bytes
    nvme_reg_write32(NVME_REG_CC, cc);

    if (!nvme_wait_ready(true)) {
        serial_write_str("[NVME] Controller did not reach RDY\n");
        return false;
    }

    if (!nvme_admin_set_features_num_queues(1)) {
        serial_write_str("[NVME] Failed to negotiate IO queue count\n");
        return false;
    }

    g_nvme.io_q.qid = NVME_IO_QID;
    g_nvme.io_q.depth = NVME_IO_Q_DEPTH;
    g_nvme.io_q.sq_tail = 0;
    g_nvme.io_q.cq_head = 0;
    g_nvme.io_q.cq_phase = 1;
    g_nvme.io_q.sq = nvme_alloc_page(&g_nvme.io_q.sq_phys);
    g_nvme.io_q.cq = nvme_alloc_page(&g_nvme.io_q.cq_phys);

    if (!g_nvme.io_q.sq || !g_nvme.io_q.cq) {
        serial_write_str("[NVME] Failed to allocate IO queues\n");
        return false;
    }

    if (!nvme_admin_create_io_cq(&g_nvme.io_q)) {
        serial_write_str("[NVME] Failed CREATE_IO_CQ\n");
        return false;
    }

    if (!nvme_admin_create_io_sq(&g_nvme.io_q)) {
        serial_write_str("[NVME] Failed CREATE_IO_SQ\n");
        return false;
    }

    return true;
}

static bool nvme_identify_namespace(void) {
    uint64_t ctrl_phys = 0;
    uint8_t* ctrl = (uint8_t*)nvme_alloc_page(&ctrl_phys);
    if (!ctrl) {
        return false;
    }

    if (!nvme_admin_identify(0, NVME_IDENTIFY_CNS_CONTROLLER, ctrl_phys)) {
        serial_write_str("[NVME] IDENTIFY controller failed\n");
        return false;
    }

    uint32_t nn = *(uint32_t*)(ctrl + 516);
    if (nn == 0) {
        serial_write_str("[NVME] Controller reports zero namespaces\n");
        return false;
    }

    uint64_t list_phys = 0;
    uint32_t* ns_list = (uint32_t*)nvme_alloc_page(&list_phys);
    if (!ns_list) {
        return false;
    }

    if (!nvme_admin_identify(0, NVME_IDENTIFY_CNS_ACTIVE_NS_LIST, list_phys)) {
        serial_write_str("[NVME] IDENTIFY active namespace list failed\n");
        return false;
    }

    uint32_t nsid = 0;
    for (uint32_t i = 0; i < (PAGE_SIZE / sizeof(uint32_t)); i++) {
        if (ns_list[i] != 0) {
            nsid = ns_list[i];
            break;
        }
    }

    if (nsid == 0) {
        serial_write_str("[NVME] No active namespace found\n");
        return false;
    }

    uint64_t ns_phys = 0;
    uint8_t* ns = (uint8_t*)nvme_alloc_page(&ns_phys);
    if (!ns) {
        return false;
    }

    if (!nvme_admin_identify(nsid, NVME_IDENTIFY_CNS_NAMESPACE, ns_phys)) {
        serial_write_str("[NVME] IDENTIFY namespace failed\n");
        return false;
    }

    uint64_t nsze = *(uint64_t*)(ns + 0);
    if (nsze == 0) {
        serial_write_str("[NVME] Namespace size is zero\n");
        return false;
    }

    uint8_t flbas = *(uint8_t*)(ns + 26);
    uint8_t format_idx = flbas & 0x0F;

    uint32_t lbaf = *(uint32_t*)(ns + 128 + (format_idx * 4));
    uint8_t lbads = (uint8_t)((lbaf >> 16) & 0xFF);
    uint32_t block_size = (uint32_t)(1U << lbads);

    if (block_size == 0 || block_size > PAGE_SIZE) {
        serial_printf("[NVME] Unsupported LBA size %u\n", block_size);
        return false;
    }

    g_nvme.namespace.present = true;
    g_nvme.namespace.namespace_id = nsid;
    g_nvme.namespace.block_size = block_size;
    g_nvme.namespace.total_blocks = nsze;

    g_nvme.bounce = nvme_alloc_page(&g_nvme.bounce_phys);
    if (!g_nvme.bounce) {
        return false;
    }

    serial_printf("[NVME] Namespace %u blocks=%u lba_size=%u\n",
                  nsid,
                  (uint32_t)nsze,
                  block_size);
    return true;
}

void nvme_init(void) {
    pci_device_info_t device = {0};
    if (!pci_find_first_by_class(NVME_PCI_CLASS, NVME_PCI_SUBCLASS, NVME_PCI_PROGIF, &device)) {
        serial_write_str("[NVME] No NVMe controller found\n");
        return;
    }

    uint32_t command = pci_config_read_u32(device.bus, device.device, device.function, 0x04);
    command |= (1U << 1); // Memory space enable
    command |= (1U << 2); // Bus master enable
    pci_config_write_u32(device.bus, device.device, device.function, 0x04, command);

    bool is_mem = false;
    bool is_64 = false;
    uint64_t bar0 = pci_read_bar(device.bus, device.device, device.function, 0, &is_mem, &is_64);

    if (!is_mem || bar0 == 0) {
        serial_write_str("[NVME] Invalid BAR0 for NVMe controller\n");
        return;
    }

    nvme_map_mmio_range(bar0, NVME_MMIO_MAP_SIZE);
    g_nvme.mmio = (volatile uint8_t*)phys_to_virt(bar0);
    g_nvme.pci = device;

    uint64_t cap = nvme_reg_read64(NVME_REG_CAP);
    uint32_t version = nvme_reg_read32(NVME_REG_VS);
    g_nvme.doorbell_stride_bytes = (uint32_t)(4U << ((cap >> 32) & 0xF));

    serial_printf("[NVME] Found %x:%x.%x vendor=%x device=%x cap=0x%x vs=0x%x bar64=%u\n",
                  device.bus,
                  device.device,
                  device.function,
                  device.vendor_id,
                  device.device_id,
                  cap,
                  version,
                  (uint32_t)is_64);

    if (!nvme_controller_enable()) {
        serial_write_str("[NVME] Controller init failed\n");
        return;
    }

    if (!nvme_identify_namespace()) {
        serial_write_str("[NVME] Namespace identify failed\n");
        return;
    }

    g_nvme.initialized = true;
    serial_write_str("[NVME] Driver initialized\n");
}

bool nvme_is_ready(void) {
    return g_nvme.initialized && g_nvme.namespace.present;
}

const nvme_namespace_t* nvme_get_namespace(void) {
    if (!nvme_is_ready()) {
        return NULL;
    }

    return &g_nvme.namespace;
}

static bool nvme_rw_one(bool write, uint64_t lba, const void* in, void* out) {
    nvme_sqe_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = write ? NVME_NVM_OP_WRITE : NVME_NVM_OP_READ;
    cmd.command_id = nvme_next_cid();
    cmd.nsid = g_nvme.namespace.namespace_id;
    cmd.prp1 = g_nvme.bounce_phys;
    cmd.cdw10 = (uint32_t)(lba & 0xFFFFFFFFULL);
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = 0; 

    uint32_t bs = g_nvme.namespace.block_size;

    if (write) {
        memcpy(g_nvme.bounce, in, bs);
    }

    if (!nvme_submit_and_wait(&g_nvme.io_q, &cmd, NULL)) {
        return false;
    }

    if (!write) {
        memcpy(out, g_nvme.bounce, bs);
    }

    return true;
}

bool nvme_read_blocks(uint64_t lba, uint32_t block_count, void* buffer) {
    if (!nvme_is_ready() || !buffer || block_count == 0) {
        return false;
    }

    uint64_t end_lba = lba + (uint64_t)block_count;
    if (end_lba < lba || end_lba > g_nvme.namespace.total_blocks) {
        return false;
    }

    uint8_t* out = (uint8_t*)buffer;
    uint32_t bs = g_nvme.namespace.block_size;

    for (uint32_t i = 0; i < block_count; i++) {
        if (!nvme_rw_one(false, lba + i, NULL, out + ((size_t)i * bs))) {
            return false;
        }
    }

    return true;
}

bool nvme_write_blocks(uint64_t lba, uint32_t block_count, const void* buffer) {
    if (!nvme_is_ready() || !buffer || block_count == 0) {
        return false;
    }

    uint64_t end_lba = lba + (uint64_t)block_count;
    if (end_lba < lba || end_lba > g_nvme.namespace.total_blocks) {
        return false;
    }

    const uint8_t* in = (const uint8_t*)buffer;
    uint32_t bs = g_nvme.namespace.block_size;

    for (uint32_t i = 0; i < block_count; i++) {
        if (!nvme_rw_one(true, lba + i, in + ((size_t)i * bs), NULL)) {
            return false;
        }
    }

    return true;
}
