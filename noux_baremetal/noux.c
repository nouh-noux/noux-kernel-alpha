/*
 * Noux Core Engine & Gasty Interpreter - Bare-Metal Kernel
 * Copyright (C) 2026 Nouh Zidani (Nouh/Noux)
 */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDRESS 0xB8000
#define CMD_BUFFER_SIZE 256
#define HEAP_SIZE (4 * 1024 * 1024)

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int bool;

#define true 1
#define false 0

volatile uint16_t* vga_buffer = (volatile uint16_t*)VGA_ADDRESS;
int cursor_x = 0;
int cursor_y = 0;
uint8_t current_color = 0x0F;

char cmd_buffer[CMD_BUFFER_SIZE];
int cmd_index = 0;
bool shift_pressed = false;

// خريطة المفاتيح العادية
const char kbd_layout_normal[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};

// خريطة المفاتيح عند الضغط على Shift (تأمين ظهور * و + وغيرهما)
const char kbd_layout_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' '
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (uint16_t)' ' | ((uint16_t)current_color << 8);
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor(cursor_x, cursor_y);
}

void putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)' ' | ((uint16_t)current_color << 8);
        }
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)c | ((uint16_t)current_color << 8);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = (uint16_t)' ' | ((uint16_t)current_color << 8);
        }
        cursor_y = VGA_HEIGHT - 1;
    }
    update_cursor(cursor_x, cursor_y);
}

void print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        putchar(str[i]);
    }
}

// نظام الذاكرة الديناميكية
static uint8_t heap_memory[HEAP_SIZE];
typedef struct Header {
    uint32_t size;
    bool is_free;
    struct Header* next;
} Header;
static Header* heap_head = (Header*)heap_memory;

void init_heap() {
    heap_head->size = HEAP_SIZE - sizeof(Header);
    heap_head->is_free = true;
    heap_head->next = (void*)0;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

int gasty_atoi(const char* str) {
    int res = 0, sign = 1, i = 0;
    while (str[i] == ' ') i++;
    if (str[i] == '-') { sign = -1; i++; }
    for (; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') break;
        res = res * 10 + (str[i] - '0');
    }
    return sign * res;
}

void print_int(int num) {
    if (num == 0) { putchar('0'); return; }
    if (num < 0) { putchar('-'); num = -num; }
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    while (num > 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }
    print(&buf[i + 1]);
}

bool is_number(const char* str) {
    int i = (str[0] == '-') ? 1 : 0;
    if (str[i] == '\0') return false;
    for (; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') return false;
    }
    return true;
}

#define MAX_VARIABLES 20
#define MAX_VAR_NAME 16

typedef struct {
    char name[MAX_VAR_NAME];
    int value;
    bool is_active;
} GastyVariable;

GastyVariable gasty_env[MAX_VARIABLES];

void set_gasty_var(const char* name, int value) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (gasty_env[i].is_active && strcmp(gasty_env[i].name, name) == 0) {
            gasty_env[i].value = value;
            return;
        }
    }
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (!gasty_env[i].is_active) {
            strcpy(gasty_env[i].name, name);
            gasty_env[i].value = value;
            gasty_env[i].is_active = true;
            return;
        }
    }
}

int get_gasty_var(const char* name, bool* found) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (gasty_env[i].is_active && strcmp(gasty_env[i].name, name) == 0) {
            if (found) *found = true;
            return gasty_env[i].value;
        }
    }
    if (found) *found = false;
    return 0;
}

int resolve_value(const char* token) {
    if (is_number(token)) return gasty_atoi(token);
    bool found = false;
    return get_gasty_var(token, &found);
}

void process_command(char* cmd);

void parse_gasty_print(char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') return;
    bool found = false;
    int val = get_gasty_var(args, &found);
    if (found || is_number(args)) {
        if (!found) val = gasty_atoi(args);
        print_int(val);
        print("\n");
    } else {
        print(args);
        print("\n");
    }
}

void parse_set(char* args) {
    char var_name[MAX_VAR_NAME] = {0};
    char val_str[32] = {0};
    int i = 0, j = 0;
    while (args[i] == ' ') i++;
    while (args[i] != ' ' && args[i] != '=' && args[i] != '\0') {
        if (j < MAX_VAR_NAME - 1) var_name[j++] = args[i];
        i++;
    }
    var_name[j] = '\0';
    while (args[i] == ' ' || args[i] == '=') i++;
    j = 0;
    while (args[i] != '\0') {
        if (j < 31) val_str[j++] = args[i];
        i++;
    }
    val_str[j] = '\0';
    if (strlen(var_name) > 0 && strlen(val_str) > 0) {
        int val = resolve_value(val_str);
        set_gasty_var(var_name, val);
        current_color = 0x0A;
        print("[Gasty] "); print(var_name); print(" = "); print_int(val); print("\n");
        current_color = 0x0F;
    }
}

void parse_calc(char* args) {
    char token1[32] = {0}, op = 0, token2[32] = {0};
    int i = 0, j = 0;
    while (args[i] == ' ') i++;
    while (args[i] != ' ' && args[i] != '\0') token1[j++] = args[i++];
    token1[j] = '\0';
    while (args[i] == ' ') i++;
    if (args[i] != '\0') op = args[i++];
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != '\0') token2[j++] = args[i++];
    token2[j] = '\0';
    if (strlen(token1) > 0 && op != 0 && strlen(token2) > 0) {
        int val1 = resolve_value(token1);
        int val2 = resolve_value(token2);
        int result = 0;
        if (op == '+') result = val1 + val2;
        else if (op == '-') result = val1 - val2;
        else if (op == '*') result = val1 * val2;
        else if (op == '/') {
            if (val2 == 0) { print("Error: Div by zero!\n"); return; }
            result = val1 / val2;
        }
        current_color = 0x0B;
        print("Result = "); print_int(result); print("\n");
        current_color = 0x0F;
    }
}

void parse_if(char* args) {
    char token1[32] = {0}, op[3] = {0}, token2[32] = {0}, action[128] = {0};
    int i = 0, j = 0;
    while (args[i] == ' ') i++;
    while (args[i] != ' ' && args[i] != '\0') token1[j++] = args[i++];
    token1[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != ' ' && args[i] != '\0' && j < 2) op[j++] = args[i++];
    op[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != ' ' && args[i] != '\0') token2[j++] = args[i++];
    token2[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != '\0') action[j++] = args[i++];
    action[j] = '\0';
    if (strlen(token1) > 0 && strlen(op) > 0 && strlen(token2) > 0 && strlen(action) > 0) {
        int val1 = resolve_value(token1);
        int val2 = resolve_value(token2);
        bool condition_met = false;
        if (strcmp(op, "==") == 0) condition_met = (val1 == val2);
        else if (strcmp(op, ">") == 0) condition_met = (val1 > val2);
        else if (strcmp(op, "<") == 0) condition_met = (val1 < val2);
        else if (strcmp(op, "!=") == 0) condition_met = (val1 != val2);
        if (condition_met) process_command(action);
    }
}

void process_command(char* cmd) {
    if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } 
    else if (strcmp(cmd, "info") == 0) {
        current_color = 0x0B;
        print("Kernel Name : Noux Bare-Metal OS Core\n");
        print("Developer   : Nouh Zidani (Nouh/Noux)\n");
        current_color = 0x0F;
    }
    else if (strcmp(cmd, "vars") == 0) {
        for (int i = 0; i < MAX_VARIABLES; i++) {
            if (gasty_env[i].is_active) {
                print("  "); print(gasty_env[i].name); print(" = "); print_int(gasty_env[i].value); print("\n");
            }
        }
    }
    else if (strncmp(cmd, "gasty ", 6) == 0) {
        char* gasty_cmd = cmd + 6;
        if (strncmp(gasty_cmd, "print ", 6) == 0) {
            parse_gasty_print(gasty_cmd + 6);
        }
        else if (strncmp(gasty_cmd, "set ", 4) == 0) {
            parse_set(gasty_cmd + 4);
        }
        else if (strncmp(gasty_cmd, "calc ", 5) == 0) {
            parse_calc(gasty_cmd + 5);
        }
        else if (strncmp(gasty_cmd, "if ", 3) == 0) {
            parse_if(gasty_cmd + 3);
        }
        else {
            current_color = 0x0C;
            print("Gasty Error: Unknown command -> "); print(gasty_cmd); print("\n");
            current_color = 0x0F;
        }
    }
    else if (strcmp(cmd, "help") == 0) {
        print("Commands:\n");
        print("  gasty print <text/var>     : Print via Gasty\n");
        print("  gasty set <var> = <val>    : Set variable via Gasty\n");
        print("  gasty calc <a> <op> <b>    : Math via Gasty (+, -, *, /)\n");
        print("  gasty if <a> <op> <b> <cmd>: Condition via Gasty\n");
        print("  vars / info / clear\n");
    }
    else if (cmd[0] != '\0') {
        current_color = 0x0C;
        print("Noux Error: Command not recognized -> "); print(cmd); print("\n");
        current_color = 0x0F;
    }
}

void keyboard_handler() {
    if (inb(0x64) & 1) {
        uint8_t scancode = inb(0x60);
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
            return;
        }
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = false;
            return;
        }
        if (!(scancode & 0x80)) {
            char c = shift_pressed ? kbd_layout_shift[scancode] : kbd_layout_normal[scancode];
            if (c == '\n') {
                putchar('\n');
                cmd_buffer[cmd_index] = '\0';
                process_command(cmd_buffer);
                cmd_index = 0;
                print("nouh@NouxCore_BareMetal:~$ ");
            } 
            else if (c == '\b') {
                if (cmd_index > 0) {
                    cmd_index--;
                    putchar('\b');
                }
            } 
            else if (c != 0 && cmd_index < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_index++] = c;
                putchar(c);
            }
        }
    }
}

void kernel_main(uint32_t magic, uint32_t addr) {
    (void)magic; (void)addr;
    init_heap();
    clear_screen();
    current_color = 0x0A;
    print("===================================================\n");
    print("    Welcome to Noux Bare-Metal OS Kernel v1.0      \n");
    print("      Developed by Nouh Zidani (Nouh/Noux)         \n");
    print("===================================================\n\n");
    
    current_color = 0x0F;
    for (int i = 0; i < MAX_VARIABLES; i++) gasty_env[i].is_active = false;

    print("Gasty engine ready. Type 'help' to see commands.\n\n");
    print("nouh@NouxCore_BareMetal:~$ ");

    while (1) {
        keyboard_handler();
    }
}
