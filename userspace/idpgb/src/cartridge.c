#include <cartridge.h>
#include <libidp/syscall.h>
#include <libidp/stdio.h>
#include <libidp/fs.h>
#include <libidp/string.h>
#include <libidp/heap.h>
#include <libidp/ansi.h>

typedef struct {
    char filename[1024];
    uint32_t rom_size;
    uint8_t* rom_data;
    rom_header_t* header;
} cartridge_ctx_t;

static cartridge_ctx_t cartridge_ctx;

static const char *ROM_TYPES[] = {
    "ROM ONLY",
    "MBC1",
    "MBC1+RAM",
    "MBC1+RAM+BATTERY",
    "0x04 ???",
    "MBC2",
    "MBC2+BATTERY",
    "0x07 ???",
    "ROM+RAM 1",
    "ROM+RAM+BATTERY 1",
    "0x0A ???",
    "MMM01",
    "MMM01+RAM",
    "MMM01+RAM+BATTERY",
    "0x0E ???",
    "MBC3+TIMER+BATTERY",
    "MBC3+TIMER+RAM+BATTERY 2",
    "MBC3",
    "MBC3+RAM 2",
    "MBC3+RAM+BATTERY 2",
    "0x14 ???",
    "0x15 ???",
    "0x16 ???",
    "0x17 ???",
    "0x18 ???",
    "MBC5",
    "MBC5+RAM",
    "MBC5+RAM+BATTERY",
    "MBC5+RUMBLE",
    "MBC5+RUMBLE+RAM",
    "MBC5+RUMBLE+RAM+BATTERY",
    "0x1F ???",
    "MBC6",
    "0x21 ???",
    "MBC7+SENSOR+RUMBLE+RAM+BATTERY",
};

static const char *LIC_CODE[0xA5] = {
    [0x00] = "None",
    [0x01] = "Nintendo R&D1",
    [0x08] = "Capcom",
    [0x13] = "Electronic Arts",
    [0x18] = "Hudson Soft",
    [0x19] = "b-ai",
    [0x20] = "kss",
    [0x22] = "pow",
    [0x24] = "PCM Complete",
    [0x25] = "san-x",
    [0x28] = "Kemco Japan",
    [0x29] = "seta",
    [0x30] = "Viacom",
    [0x31] = "Nintendo",
    [0x32] = "Bandai",
    [0x33] = "Ocean/Acclaim",
    [0x34] = "Konami",
    [0x35] = "Hector",
    [0x37] = "Taito",
    [0x38] = "Hudson",
    [0x39] = "Banpresto",
    [0x41] = "Ubi Soft",
    [0x42] = "Atlus",
    [0x44] = "Malibu",
    [0x46] = "angel",
    [0x47] = "Bullet-Proof",
    [0x49] = "irem",
    [0x50] = "Absolute",
    [0x51] = "Acclaim",
    [0x52] = "Activision",
    [0x53] = "American sammy",
    [0x54] = "Konami",
    [0x55] = "Hi tech entertainment",
    [0x56] = "LJN",
    [0x57] = "Matchbox",
    [0x58] = "Mattel",
    [0x59] = "Milton Bradley",
    [0x60] = "Titus",
    [0x61] = "Virgin",
    [0x64] = "LucasArts",
    [0x67] = "Ocean",
    [0x69] = "Electronic Arts",
    [0x70] = "Infogrames",
    [0x71] = "Interplay",
    [0x72] = "Broderbund",
    [0x73] = "sculptured",
    [0x75] = "sci",
    [0x78] = "THQ",
    [0x79] = "Accolade",
    [0x80] = "misawa",
    [0x83] = "lozc",
    [0x86] = "Tokuma Shoten Intermedia",
    [0x87] = "Tsukuda Original",
    [0x91] = "Chunsoft",
    [0x92] = "Video system",
    [0x93] = "Ocean/Acclaim",
    [0x95] = "Varie",
    [0x96] = "Yonezawa/s’pal",
    [0x97] = "Kaneko",
    [0x99] = "Pack in soft",
    [0xA4] = "Konami (Yu-Gi-Oh!)"
};

const char *cart_lic_name() {
    if (cartridge_ctx.header->new_lic_code <= 0xA4) {
        return LIC_CODE[cartridge_ctx.header->old_lic_code];
    }

    return "UNKNOWN";
}

const char *cart_type_name() {
    if (cartridge_ctx.header->type <= 0x22) {
        return ROM_TYPES[cartridge_ctx.header->type];
    }

    return "UNKNOWN";
}

bool cartridge_load(char* path) {
    strlcpy(cartridge_ctx.filename, path, sizeof(cartridge_ctx.filename));
    
    file_t file;
    if (fs_open(&file, path, IDP_O_RDONLY) != ERR_SUCCESS) {
        printf("idpgb: error opening cartridge file %s.\n", path);
        return false;
    }
    printf("idpgb: opened rom file %s\n", path);

    uint32_t total_size = 0;
    char buf[512];
    size_t read_bytes = 0;
    
    while (fs_read(&file, buf, sizeof(buf), &read_bytes) == ERR_SUCCESS) {
        if (read_bytes == 0) break;
        total_size += read_bytes;
    }
    fs_close(&file);
    
    cartridge_ctx.rom_size = total_size;

    if (cartridge_ctx.rom_size == 0) {
        printf("idpgb: cartridge file is empty!\n");
        return false;
    }

    cartridge_ctx.rom_data = (uint8_t*)malloc(cartridge_ctx.rom_size);
    if (!cartridge_ctx.rom_data) {
        printf("idpgb: failed to allocate memory for rom.\n");
        return false;
    }

    if (fs_open(&file, path, IDP_O_RDONLY) != ERR_SUCCESS) {
        printf("idpgb: failed to reopen cartridge file for data read.\n");
        free(cartridge_ctx.rom_data);
        return false;
    }

    size_t offset = 0;
    while (offset < cartridge_ctx.rom_size) {
        if (fs_read(&file, cartridge_ctx.rom_data + offset, cartridge_ctx.rom_size - offset, &read_bytes) != ERR_SUCCESS) {
            break;
        }
        if (read_bytes == 0) break;
        offset += read_bytes;
    }
    fs_close(&file);

    cartridge_ctx.header = (rom_header_t*)(cartridge_ctx.rom_data + 0x100);

    uint16_t x = 0;
    for (uint16_t i = 0x0134; i <= 0x014C; i++) {
        x = x - cartridge_ctx.rom_data[i] - 1;
    }
    bool checksum_passed = (x & 0xFF) == cartridge_ctx.header->checksum;

    char safe_title[17] = {0};
    for (int i = 0; i < 16; i++) {
        safe_title[i] = cartridge_ctx.header->title[i];
    }

    printf(ANSI_FG_BRIGHT_MAGENTA"Cartridge Loaded:\n"ANSI_RESET);
    printf("  Title    : "ANSI_FG_YELLOW"%s"ANSI_RESET"\n", safe_title);
    printf("  Type     : "ANSI_FG_YELLOW"%x"ANSI_RESET" (%s)\n", cartridge_ctx.header->type, cart_type_name());
    printf("  ROM Size : "ANSI_FG_YELLOW"%d"ANSI_RESET" KB\n", 32 << cartridge_ctx.header->rom_size);
    printf("  RAM Size : "ANSI_FG_YELLOW"%x"ANSI_RESET"\n", cartridge_ctx.header->ram_size);
    printf("  LIC Code : "ANSI_FG_YELLOW"%x"ANSI_RESET" (%s)\n", cartridge_ctx.header->old_lic_code, cart_lic_name());
    printf("  ROM Vers : "ANSI_FG_YELLOW"%x"ANSI_RESET"\n", cartridge_ctx.header->version);
    printf("  Checksum : "ANSI_FG_YELLOW"%x"ANSI_RESET" (%s)\n", cartridge_ctx.header->checksum, checksum_passed ? ANSI_FG_GREEN"PASSED"ANSI_RESET : ANSI_FG_RED"FAILED"ANSI_RESET);
    
    return true;
}