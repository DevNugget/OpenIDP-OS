#include <libidp/stdio.h>
#include <libidp/syscall.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_SOURCE_SIZE (1024*1024)
#define MAX_TOKENS 65536
#define MAX_TEXT_SIZE (1024*1024)
#define MAX_DATA_SIZE (1024*1024)
#define MAX_SYMBOLS 1024
#define MAX_RELOCS 2048
#define MAX_ALIASES 128
#define MAX_NAME 64

typedef enum {
    TOK_EOF = 0,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_COMMA,
    TOK_STAR,
    TOK_PLUS,
    TOK_MINUS,
} token_type_t;

typedef struct {
    token_type_t kind;
    const char* start;
    uint32_t len;
    uint64_t number;
} token_t;

typedef enum {
	SECTION_TEXT = 1,
	SECTION_DATA,
} section_t;

typedef struct {
    char name[MAX_NAME];
   	section_t section;
    uint64_t offset;
} obj_symbol_t;

typedef struct {
    section_t section;
    uint64_t offset;
    uint32_t symbol_index;
} obj_reloc_t;

typedef struct {
    char alias[MAX_NAME];
    char reg[MAX_NAME];
} alias_t;

static char g_source[MAX_SOURCE_SIZE];
static uint64_t g_source_size;
static token_t g_tokens[MAX_TOKENS];
static uint32_t g_token_count;
static uint32_t g_tok_idx;

static uint8_t g_text[MAX_TEXT_SIZE];
static uint8_t g_data[MAX_DATA_SIZE];
static uint64_t g_text_size;
static uint64_t g_data_size;

static obj_symbol_t g_symbols[MAX_SYMBOLS];
static uint32_t g_symbol_count;
static obj_reloc_t g_relocs[MAX_RELOCS];
static uint32_t g_reloc_count;

static alias_t g_aliases[MAX_ALIASES];
static uint32_t g_alias_count;

static section_t g_current_section;

static int streqn(const char* a, uint32_t alen, const char* b) {
    uint32_t i = 0;
    while (b[i] != 0) {
        if (i >= alen || a[i] != b[i]) return 0;
        i++;
    }
    return i == alen;
}

static uint32_t cstrlen(const char* s) {
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

static void copy_name(char* dst, const char* src, uint32_t len) {
    uint32_t n = len;
    if (n >= MAX_NAME) n = MAX_NAME - 1;
    for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
    dst[n] = 0;
}

static void fatal(const char* msg) {
    printf("idpasm: %s\n", msg);
    sys_exit(1);
}

static void emit_byte(uint8_t b) {
    if (g_current_section == 1) {
        if (g_text_size >= MAX_TEXT_SIZE) fatal("text section overflow");
        g_text[g_text_size++] = b;
    } else if (g_current_section == 2) {
        if (g_data_size >= MAX_DATA_SIZE) fatal("data section overflow");
        g_data[g_data_size++] = b;
    } else {
        fatal("statement outside section");
    }
}

static void emit_u16(uint16_t v) {
    emit_byte((uint8_t)(v & 0xFF));
    emit_byte((uint8_t)((v >> 8) & 0xFF));
}

static void emit_u32(uint32_t v) {
    for (int i = 0; i < 4; i++) emit_byte((uint8_t)((v >> (i * 8)) & 0xFF));
}

static void emit_u64(uint64_t v) {
    for (int i = 0; i < 8; i++) emit_byte((uint8_t)((v >> (i * 8)) & 0xFF));
}

static uint64_t section_offset(void) {
    if (g_current_section == SECTION_TEXT) return g_text_size;
    if (g_current_section == SECTION_DATA) return g_data_size;
    return 0;
}

static void add_symbol_tok(token_t t) {
    if (g_symbol_count >= MAX_SYMBOLS) fatal("too many symbols");
    for (uint32_t i = 0; i < g_symbol_count; i++) {
        if (streqn(t.start, t.len, g_symbols[i].name)) {
        	fatal("duplicate symbol");
        	printf("%s\n", g_symbols[i].name);
        }
    }
    copy_name(g_symbols[g_symbol_count].name, t.start, t.len);
    g_symbols[g_symbol_count].section = g_current_section;
    g_symbols[g_symbol_count].offset = section_offset();
    g_symbol_count++;
}

static uint32_t find_symbol_tok(token_t t) {
    for (uint32_t i = 0; i < g_symbol_count; i++) {
        if (streqn(t.start, t.len, g_symbols[i].name)) return i;
    }
    return (uint32_t)-1;
}

static int read_file(const char* path, char* out, uint64_t cap, uint64_t* out_size) {
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
        uint64_t out_n = 0;
        int r = sys_write(fd, data + off, size - off, &out_n);
        if (r != ERR_SUCCESS) {
            sys_close(fd);
            return -2;
        }
        if (out_n == 0) {
            sys_close(fd);
            return -3;
        }
        off += out_n;
    }
    sys_close(fd);
    return 0;
}

static int parse_register(token_t t) {
    static const char* regs[] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8","r9","r10","r11","r12","r13","r14","r15"
    };

    for (uint32_t i = 0; i < g_alias_count; i++) {
        if (streqn(t.start, t.len, g_aliases[i].alias)) {
            uint32_t len = cstrlen(g_aliases[i].reg);
            t.start = g_aliases[i].reg;
            t.len = len;
            break;
        }
    }

    for (int i = 0; i < 16; i++) {
        if (streqn(t.start, t.len, regs[i])) return i;
    }
    return -1;
}

static token_t* peek(void) {
    if (g_tok_idx >= g_token_count) return &g_tokens[g_token_count - 1];
    return &g_tokens[g_tok_idx];
}

static token_t* next(void) {
    token_t* t = peek();
    if (g_tok_idx < g_token_count) g_tok_idx++;
    return t;
}

static int accept(token_type_t k) {
    if (peek()->kind == k) {
        next();
        return 1;
    }
    return 0;
}

static token_t expect(token_type_t k, const char* msg) {
    token_t t = *next();
    if (t.kind != k) fatal(msg);
    return t;
}

static uint64_t parse_number_expr(void) {
    token_t t = expect(TOK_NUMBER, "expected number");
    return t.number;
}

static void add_reloc(uint8_t section, uint64_t off, uint32_t sym_idx) {
    if (g_reloc_count >= MAX_RELOCS) fatal("too many relocations");
    g_relocs[g_reloc_count].section = section;
    g_relocs[g_reloc_count].offset = off;
    g_relocs[g_reloc_count].symbol_index = sym_idx;
    g_reloc_count++;
}

static void parse_def_stmt(void) {
    expect(TOK_LPAREN, "expected ( after def");
    token_t reg = expect(TOK_IDENT, "expected register name");
    expect(TOK_COMMA, "expected ,");
    token_t alias = expect(TOK_IDENT, "expected alias name");
    expect(TOK_RPAREN, "expected )");

    if (g_alias_count >= MAX_ALIASES) fatal("too many aliases");
    int r = parse_register(reg);
    if (r < 0) fatal("def first argument must be register");

    copy_name(g_aliases[g_alias_count].alias, alias.start, alias.len);

    static const char* regs[] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8","r9","r10","r11","r12","r13","r14","r15"
    };
    copy_name(g_aliases[g_alias_count].reg, regs[r], cstrlen(regs[r]));
    g_alias_count++;
}

static void parse_mov_stmt(void) {
    expect(TOK_LPAREN, "expected (");
    token_t dst = expect(TOK_IDENT, "expected destination register");
    expect(TOK_COMMA, "expected ,");
    token_t src = *next();
    expect(TOK_RPAREN, "expected )");

    int reg = parse_register(dst);
    if (reg < 0) fatal("mov destination must be register");

    uint8_t rex = 0x48;
    if (reg >= 8) rex |= 0x01;
    emit_byte(rex);
    emit_byte((uint8_t)(0xB8 + (reg & 7)));

    if (src.kind == TOK_NUMBER) {
        emit_u64(src.number);
    } else if (src.kind == TOK_IDENT) {
        uint32_t sym = find_symbol_tok(src);
        if (sym == (uint32_t)-1) {
            if (g_symbol_count >= MAX_SYMBOLS) fatal("too many symbols");
            copy_name(g_symbols[g_symbol_count].name, src.start, src.len);
            g_symbols[g_symbol_count].section = 0;
            g_symbols[g_symbol_count].offset = 0;
            sym = g_symbol_count++;
        }
        uint64_t imm_off = section_offset();
        emit_u64(0);
        add_reloc(g_current_section, imm_off, sym);
    } else {
        fatal("mov source must be number or symbol");
    }
}

static void parse_raw_stmt(void) {
    expect(TOK_LPAREN, "expected (");
    for (;;) {
        token_t t = *next();
        if (t.kind == TOK_NUMBER) {
            emit_byte((uint8_t)(t.number & 0xFF));
        } else if (t.kind == TOK_STRING) {
            for (uint32_t i = 0; i < t.len; i++) emit_byte((uint8_t)t.start[i]);
        } else {
            fatal("raw only accepts numbers and strings");
        }
        if (accept(TOK_COMMA)) continue;
        break;
    }
    expect(TOK_RPAREN, "expected )");
}

static void parse_align_stmt(void) {
    expect(TOK_LPAREN, "expected (");
    uint64_t n = parse_number_expr();
    expect(TOK_RPAREN, "expected )");
    if (n == 0) return;
    while ((section_offset() % n) != 0) emit_byte(0);
}

static void parse_data_emit(const char* kw) {
    expect(TOK_LPAREN, "expected (");
    uint64_t v = parse_number_expr();
    expect(TOK_RPAREN, "expected )");
    if (cstreq(kw, "u8")) emit_byte((uint8_t)v);
    else if (cstreq(kw, "u16")) emit_u16((uint16_t)v);
    else if (cstreq(kw, "u32")) emit_u32((uint32_t)v);
    else if (cstreq(kw, "u64")) emit_u64(v);
}

static void parse_statement(void);

static void parse_block(void) {
    expect(TOK_LBRACE, "expected {");
    while (!accept(TOK_RBRACE)) {
        if (peek()->kind == TOK_EOF) fatal("unexpected EOF in block");
        parse_statement();
    }
}

static void parse_label_stmt(void) {
    expect(TOK_LPAREN, "expected (");
    token_t name = expect(TOK_IDENT, "expected label name");
    expect(TOK_RPAREN, "expected )");
    add_symbol_tok(name);
    parse_block();
}

static void parse_section_stmt(void) {
    expect(TOK_LPAREN, "expected (");
    token_t name = expect(TOK_IDENT, "expected section name");
    expect(TOK_RPAREN, "expected )");

    if (streqn(name.start, name.len, "text")) g_current_section = 1;
    else if (streqn(name.start, name.len, "data")) g_current_section = 2;
    else fatal("only text and data sections are currently supported");

    parse_block();
    g_current_section = 0;
}

static void parse_statement(void) {
    token_t id = expect(TOK_IDENT, "expected statement");

    if (streqn(id.start, id.len, "section")) parse_section_stmt();
    else if (streqn(id.start, id.len, "label")) parse_label_stmt();
    else if (streqn(id.start, id.len, "mov")) parse_mov_stmt();
    else if (streqn(id.start, id.len, "syscall")) {
        expect(TOK_LPAREN, "expected (");
        expect(TOK_RPAREN, "expected )");
        emit_byte(0xCD); emit_byte(0x80);
    } else if (streqn(id.start, id.len, "ret")) {
        expect(TOK_LPAREN, "expected (");
        expect(TOK_RPAREN, "expected )");
        emit_byte(0xC3);
    } else if (streqn(id.start, id.len, "raw")) parse_raw_stmt();
    else if (streqn(id.start, id.len, "align")) parse_align_stmt();
    else if (streqn(id.start, id.len, "def")) parse_def_stmt();
    else if (streqn(id.start, id.len, "u8")) parse_data_emit("u8");
    else if (streqn(id.start, id.len, "u16")) parse_data_emit("u16");
    else if (streqn(id.start, id.len, "u32")) parse_data_emit("u32");
    else if (streqn(id.start, id.len, "u64")) parse_data_emit("u64");
    else fatal("unsupported statement");
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_hex(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static void push_tok(token_type_t kind, const char* start, uint32_t len, uint64_t num) {
    if (g_token_count >= MAX_TOKENS) fatal("too many tokens");
    g_tokens[g_token_count].kind = kind;
    g_tokens[g_token_count].start = start;
    g_tokens[g_token_count].len = len;
    g_tokens[g_token_count].number = num;
    g_token_count++;
}

static void lex(void) {
    uint64_t i = 0;
    while (i < g_source_size) {
        char c = g_source[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
        if (c == ';') {
            while (i < g_source_size && g_source[i] != '\n') i++;
            continue;
        }
        if (is_alpha(c)) {
            uint64_t s = i;
            i++;
            while (i < g_source_size && (is_alpha(g_source[i]) || is_digit(g_source[i]))) i++;
            push_tok(TOK_IDENT, g_source + s, (uint32_t)(i - s), 0);
            continue;
        }
        if (is_digit(c)) {
            uint64_t s = i;
            uint64_t v = 0;
            if (i + 1 < g_source_size && g_source[i] == '0' && (g_source[i + 1] == 'x' || g_source[i + 1] == 'X')) {
                i += 2;
                while (i < g_source_size && is_hex(g_source[i])) {
                    char h = g_source[i++];
                    v <<= 4;
                    if (h >= '0' && h <= '9') v += (uint64_t)(h - '0');
                    else if (h >= 'a' && h <= 'f') v += (uint64_t)(h - 'a' + 10);
                    else v += (uint64_t)(h - 'A' + 10);
                }
            } else {
                while (i < g_source_size && is_digit(g_source[i])) {
                    v = v * 10 + (uint64_t)(g_source[i] - '0');
                    i++;
                }
            }
            push_tok(TOK_NUMBER, g_source + s, (uint32_t)(i - s), v);
            continue;
        }
        if (c == '"') {
            i++;
            uint64_t s = i;
            while (i < g_source_size && g_source[i] != '"') i++;
            if (i >= g_source_size) fatal("unterminated string");
            push_tok(TOK_STRING, g_source + s, (uint32_t)(i - s), 0);
            i++;
            continue;
        }

        token_type_t k = TOK_EOF;
        if (c == '(') k = TOK_LPAREN;
        else if (c == ')') k = TOK_RPAREN;
        else if (c == '{') k = TOK_LBRACE;
        else if (c == '}') k = TOK_RBRACE;
        else if (c == ',') k = TOK_COMMA;
        else if (c == '*') k = TOK_STAR;
        else if (c == '+') k = TOK_PLUS;
        else if (c == '-') k = TOK_MINUS;
        else fatal("unexpected character");

        push_tok(k, g_source + i, 1, 0);
        i++;
    }
    push_tok(TOK_EOF, g_source + g_source_size, 0, 0);
}

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t symbol_count;
    uint32_t reloc_count;
    uint64_t text_size;
    uint64_t data_size;
} __attribute__((packed)) obj_header_t;

#define OBJ_BUFFER_SIZE (2*1024*1024)
static uint8_t g_objbuf[OBJ_BUFFER_SIZE];

static void put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void put_u64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
}

static uint64_t build_object(uint8_t* out) {
    obj_header_t* h = (obj_header_t*)out;
    h->magic[0] = 'I'; h->magic[1] = 'D'; h->magic[2] = 'P'; h->magic[3] = 'O';
    h->magic[4] = 'B'; h->magic[5] = 'J'; h->magic[6] = '1'; h->magic[7] = '\0';
    h->version = 1;
    h->symbol_count = g_symbol_count;
    h->reloc_count = g_reloc_count;
    h->text_size = g_text_size;
    h->data_size = g_data_size;

    uint64_t off = sizeof(obj_header_t);

    for (uint32_t i = 0; i < g_symbol_count; i++) {
        for (int j = 0; j < MAX_NAME; j++) out[off + j] = (uint8_t)g_symbols[i].name[j];
        off += MAX_NAME;
        out[off++] = g_symbols[i].section;
        put_u64(out + off, g_symbols[i].offset); off += 8;
    }

    for (uint32_t i = 0; i < g_reloc_count; i++) {
        out[off++] = g_relocs[i].section;
        put_u64(out + off, g_relocs[i].offset); off += 8;
        put_u32(out + off, g_relocs[i].symbol_index); off += 4;
    }

    for (uint64_t i = 0; i < g_text_size; i++) out[off++] = g_text[i];
    for (uint64_t i = 0; i < g_data_size; i++) out[off++] = g_data[i];
    return off;
}

void main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: idpasm <input.idpasm> <output.idpo>\n");
        sys_exit(1);
    }

    int rf = read_file(argv[1], g_source, MAX_SOURCE_SIZE, &g_source_size);
    if (rf != 0) {
        printf("idpasm: failed to read %s (error %d)\n", argv[1], rf);
        sys_exit(1);
    }

    lex();
    while (peek()->kind != TOK_EOF) parse_statement();

    uint64_t out_size = build_object(g_objbuf);
    int wf = write_file(argv[2], g_objbuf, out_size);
    if (wf != 0) {
        printf("idpasm: failed to write %s (error %d)\n", argv[2], wf);
        sys_exit(1);
    }

    printf("idpasm: wrote %s (text=%d data=%d syms=%d relocs=%d)\n",
           argv[2], g_text_size, g_data_size, g_symbol_count, g_reloc_count);
    sys_exit(0);
}