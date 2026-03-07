#include <libidp/stdio.h>
#include <libidp/syscall.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_OBJS 32
#define MAX_OBJ_SIZE (2 * 1024 * 1024)
#define MAX_TEXT_SIZE (4 * 1024 * 1024)
#define MAX_DATA_SIZE (4 * 1024 * 1024)
#define MAX_SYMBOLS 4096
#define MAX_RELOCS 8192
#define MAX_NAME 64
#define EI_NIDENT 16

#pragma pack(push, 1)
typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t symbol_count;
    uint32_t reloc_count;
    uint64_t text_size;
    uint64_t data_size;
} obj_header_t;

typedef struct {
    char name[MAX_NAME];
    uint8_t section;
    uint64_t offset;
} obj_symbol_t;

typedef struct {
    uint8_t section;
    uint64_t offset;
    uint32_t symbol_index;
} obj_reloc_t;

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;
#pragma pack(pop)

typedef struct {
    char name[MAX_NAME];
    uint64_t addr;
} global_symbol_t;

typedef struct {
    uint8_t section;
    uint64_t target_off;
    char name[MAX_NAME];
} pending_reloc_t;

static uint8_t g_objbuf[MAX_OBJ_SIZE];
static uint8_t g_text[MAX_TEXT_SIZE];
static uint8_t g_data[MAX_DATA_SIZE];
static uint64_t g_text_size;
static uint64_t g_data_size;

static global_symbol_t g_symbols[MAX_SYMBOLS];
static uint32_t g_symbol_count;
static pending_reloc_t g_relocs[MAX_RELOCS];
static uint32_t g_reloc_count;

static uint8_t g_outbuf[8 * 1024 * 1024];

static uint32_t cstr_len(const char* s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static int cstreq(const char* a, const char* b) {
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void copy_name(char* dst, const char* src) {
    uint32_t i = 0;
    for (; i + 1 < MAX_NAME && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static void copy_name_n(char* dst, const char* src, uint32_t n) {
    if (n >= MAX_NAME) n = MAX_NAME - 1;
    for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
    dst[n] = 0;
}

static void fatal(const char* msg) {
    printf("idplink: %s\n", msg);
    sys_exit(1);
}

static int read_file(const char* path, uint8_t* out, uint64_t cap, uint64_t* out_size) {
    uint64_t fd = sys_open(path, IDP_O_RDONLY);
    if (fd == (uint64_t)ERR_FAIL) return -1;

    uint64_t total = 0;
    for (;;) {
        if (total >= cap) {
            sys_close(fd);
            return -2;
        }
        uint64_t n = 0;
        int r = sys_read(fd, out + total, cap - total, &n);
        if (r != ERR_SUCCESS) {
            sys_close(fd);
            return -3;
        }
        if (n == 0) break;
        total += n;
    }
    sys_close(fd);
    *out_size = total;
    return 0;
}

static int write_file(const char* path, const uint8_t* data, uint64_t size) {
    uint64_t fd = sys_open(path, IDP_O_WRONLY | IDP_O_CREATE);
    if (fd == (uint64_t)ERR_FAIL) return -1;

    uint64_t off = 0;
    while (off < size) {
        uint64_t n = 0;
        int r = sys_write(fd, data + off, size - off, &n);
        if (r != ERR_SUCCESS) {
            sys_close(fd);
            return -2;
        }
        if (n == 0) {
            sys_close(fd);
            return -3;
        }
        off += n;
    }
    sys_close(fd);
    return 0;
}

static void put_u64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
}

static uint64_t get_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= ((uint64_t)p[i]) << (i * 8);
    return v;
}

static uint32_t get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t align_up(uint64_t x, uint64_t a) {
    if (a == 0) return x;
    return (x + (a - 1)) & ~(a - 1);
}

static uint64_t find_symbol_addr(const char* name) {
    for (uint32_t i = 0; i < g_symbol_count; i++) {
        if (cstreq(g_symbols[i].name, name)) return g_symbols[i].addr;
    }
    fatal("undefined symbol");
    return 0;
}

static void add_global_symbol(const char* name, uint64_t addr) {
    for (uint32_t i = 0; i < g_symbol_count; i++) {
        if (cstreq(g_symbols[i].name, name)) fatal("duplicate global symbol");
    }
    if (g_symbol_count >= MAX_SYMBOLS) fatal("too many symbols");
    copy_name(g_symbols[g_symbol_count].name, name);
    g_symbols[g_symbol_count].addr = addr;
    g_symbol_count++;
}

static void add_reloc(uint8_t section, uint64_t off, const char* name) {
    if (g_reloc_count >= MAX_RELOCS) fatal("too many relocations");
    g_relocs[g_reloc_count].section = section;
    g_relocs[g_reloc_count].target_off = off;
    copy_name(g_relocs[g_reloc_count].name, name);
    g_reloc_count++;
}

static void ingest_object(const char* path) {
    uint64_t size = 0;
    int rr = read_file(path, g_objbuf, MAX_OBJ_SIZE, &size);
    if (rr != 0) fatal("failed to read object file");
    if (size < sizeof(obj_header_t)) fatal("object too small");

    const obj_header_t* h = (const obj_header_t*)g_objbuf;
    if (!(h->magic[0] == 'I' && h->magic[1] == 'D' && h->magic[2] == 'P' && h->magic[3] == 'O')) {
        fatal("bad object magic");
    }

    uint64_t off = sizeof(obj_header_t);
    uint64_t text_base = g_text_size;
    uint64_t data_base = g_data_size;

    const uint8_t* sym_base = g_objbuf + off;
    off += (uint64_t)h->symbol_count * (MAX_NAME + 1 + 8);
    const uint8_t* rel_base = g_objbuf + off;
    off += (uint64_t)h->reloc_count * (1 + 8 + 4);

    if (off + h->text_size + h->data_size > size) fatal("truncated object");
    if (g_text_size + h->text_size > MAX_TEXT_SIZE) fatal("linked text overflow");
    if (g_data_size + h->data_size > MAX_DATA_SIZE) fatal("linked data overflow");

    for (uint64_t i = 0; i < h->text_size; i++) g_text[g_text_size++] = g_objbuf[off + i];
    off += h->text_size;
    for (uint64_t i = 0; i < h->data_size; i++) g_data[g_data_size++] = g_objbuf[off + i];

    for (uint32_t i = 0; i < h->symbol_count; i++) {
        const uint8_t* s = sym_base + (uint64_t)i * (MAX_NAME + 1 + 8);
        char name[MAX_NAME];
        copy_name_n(name, (const char*)s, cstr_len((const char*)s));
        uint8_t sec = s[MAX_NAME];
        uint64_t off_sym = get_u64(s + MAX_NAME + 1);

        if (sec == 1) add_global_symbol(name, text_base + off_sym);
        else if (sec == 2) add_global_symbol(name, (1ULL << 63) | (data_base + off_sym));
    }

    for (uint32_t i = 0; i < h->reloc_count; i++) {
        const uint8_t* r = rel_base + (uint64_t)i * (1 + 8 + 4);
        uint8_t sec = r[0];
        uint64_t roff = get_u64(r + 1);
        uint32_t sym_idx = get_u32(r + 9);
        if (sym_idx >= h->symbol_count) fatal("bad relocation symbol index");

        const uint8_t* s = sym_base + (uint64_t)sym_idx * (MAX_NAME + 1 + 8);
        char name[MAX_NAME];
        copy_name_n(name, (const char*)s, cstr_len((const char*)s));

        if (sec == 1) add_reloc(1, text_base + roff, name);
        else if (sec == 2) add_reloc(2, data_base + roff, name);
        else fatal("bad relocation section");
    }
}

static uint64_t build_elf(const char* out_path) {
    const uint64_t base_vaddr = 0x400000;
    const uint64_t page = 0x1000;

    uint64_t headers_size = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    uint64_t text_off = align_up(headers_size, page);
    uint64_t data_off = align_up(text_off + g_text_size, 16);
    uint64_t file_size = data_off + g_data_size;

    if (file_size > sizeof(g_outbuf)) fatal("output too large");

    for (uint64_t i = 0; i < file_size; i++) g_outbuf[i] = 0;

    uint64_t text_vaddr = base_vaddr + text_off;
    uint64_t data_vaddr = base_vaddr + data_off;

    for (uint32_t i = 0; i < g_symbol_count; i++) {
        if (g_symbols[i].addr & (1ULL << 63)) {
            g_symbols[i].addr = data_vaddr + (g_symbols[i].addr & ~(1ULL << 63));
        } else {
            g_symbols[i].addr = text_vaddr + g_symbols[i].addr;
        }
    }

    for (uint32_t i = 0; i < g_reloc_count; i++) {
        uint64_t addr = find_symbol_addr(g_relocs[i].name);
        if (g_relocs[i].section == 1) {
            if (g_relocs[i].target_off + 8 > g_text_size) fatal("text relocation out of range");
            put_u64(g_text + g_relocs[i].target_off, addr);
        } else {
            if (g_relocs[i].target_off + 8 > g_data_size) fatal("data relocation out of range");
            put_u64(g_data + g_relocs[i].target_off, addr);
        }
    }

    Elf64_Ehdr* eh = (Elf64_Ehdr*)g_outbuf;
    eh->e_ident[0] = 0x7F; eh->e_ident[1] = 'E'; eh->e_ident[2] = 'L'; eh->e_ident[3] = 'F';
    eh->e_ident[4] = 2;
    eh->e_ident[5] = 1;
    eh->e_ident[6] = 1;
    eh->e_type = 2;
    eh->e_machine = 62;
    eh->e_version = 1;
    eh->e_entry = find_symbol_addr("_start");
    eh->e_phoff = sizeof(Elf64_Ehdr);
    eh->e_ehsize = sizeof(Elf64_Ehdr);
    eh->e_phentsize = sizeof(Elf64_Phdr);
    eh->e_phnum = 1;

    Elf64_Phdr* ph = (Elf64_Phdr*)(g_outbuf + eh->e_phoff);
    ph->p_type = 1;
    ph->p_flags = 0x7;
    ph->p_offset = 0;
    ph->p_vaddr = base_vaddr;
    ph->p_paddr = base_vaddr;
    ph->p_filesz = file_size;
    ph->p_memsz = file_size;
    ph->p_align = page;

    for (uint64_t i = 0; i < g_text_size; i++) g_outbuf[text_off + i] = g_text[i];
    for (uint64_t i = 0; i < g_data_size; i++) g_outbuf[data_off + i] = g_data[i];

    int wr = write_file(out_path, g_outbuf, file_size);
    if (wr != 0) fatal("failed to write output elf");
    return file_size;
}

void main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: idplink <output.elf> <input1.idpo> [input2.idpo ...]\n");
        sys_exit(1);
    }

    for (int i = 2; i < argc; i++) {
        ingest_object(argv[i]);
    }

    uint64_t out_size = build_elf(argv[1]);
    printf("idplink: wrote %s (text=%d data=%d symbols=%d relocs=%d bytes=%d)\n",
           argv[1], g_text_size, g_data_size, g_symbol_count, g_reloc_count, out_size);
    sys_exit(0);
}
