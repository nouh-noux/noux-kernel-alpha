/*
 * Noux Core Engine & Gasty Interpreter - Bare-Metal Kernel
 * Dynamic Memory Allocation (kmalloc / kfree) Integrated
 * Copyright (C) 2026 Nouh Zidani (Nouh/Noux)
 */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDRESS 0xB8000
#define CMD_BUFFER_SIZE 256

// تحديد حجم الـ Heap في الذاكرة (4 ميجابايت)
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

const char kbd_layout[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
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

// ========================================================
// 1. نظام إدارة الذاكرة الديناميكية (Kernel Heap Allocator)
// ========================================================

// مصفوفة الـ Heap المباشرة في الذاكرة
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

// دالة حجز الذاكرة الديناميكية
void* kmalloc(uint32_t size) {
    Header* curr = heap_head;

    // محاذاة الذاكرة لـ 4 بايتبالضبط
    size = (size + 3) & ~3;

    while (curr != (void*)0) {
        if (curr->is_free && curr->size >= size) {
            // إذا كانت الكتلة أكبر من المطلوب، نقسمها
            if (curr->size >= size + sizeof(Header) + 16) {
                Header* next_block = (Header*)((uint8_t*)curr + sizeof(Header) + size);
                next_block->size = curr->size - size - sizeof(Header);
                next_block->is_free = true;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            curr->is_free = false;
            return (void*)((uint8_t*)curr + sizeof(Header));
        }
        curr = curr->next;
    }
    return (void*)0; // الذاكرة ممتلئة
}

// دالة تحرير الذاكرة
void kfree(void* ptr) {
    if (!ptr) return;

    Header* header = (Header*)((uint8_t*)ptr - sizeof(Header));
    header->is_free = true;

    // دمج الكتل الخالية المتجاورة لمنع التجزئة (Defragmentation)
    Header* curr = heap_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(Header) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

// ========================================================
// 2. دوال طباعة النصوص والأرقام
// ========================================================
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void print_hex(uint32_t num) {
    print("0x");
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        putchar(hex_chars[(num >> i) & 0x0F]);
    }
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

// ========================================================
// 3. معالجة أوامر الشل واختبار الذاكرة
// ========================================================
void process_command(char* cmd) {
    if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } 
    else if (strcmp(cmd, "meminfo") == 0) {
        current_color = 0x0B;
        print("--- Kernel Heap Memory Status ---\n");
        print("Heap Base Address : "); print_hex((uint32_t)heap_memory); print("\n");
        print("Heap Total Size   : "); print_int(HEAP_SIZE / 1024); print(" KB\n");
        current_color = 0x0F;
    }
    else if (strncmp(cmd, "alloc ", 6) == 0) {
        // تجربة حجز ذاكرة ديناميكية من الشل
        uint32_t size = 100; // افتراضي
        void* ptr = kmalloc(size);
        if (ptr) {
            current_color = 0x0A;
            print("[Heap Success] Allocated 100 bytes at Address: ");
            print_hex((uint32_t)ptr);
            print("\n");
            current_color = 0x0F;
        } else {
            current_color = 0x0C;
            print("[Heap Error] Out of Memory!\n");
            current_color = 0x0F;
        }
    }
    else if (strcmp(cmd, "help") == 0) {
        print("Commands:\n");
        print("  meminfo     : Display Kernel Heap memory state\n");
        print("  alloc       : Test dynamic allocation (kmalloc)\n");
        print("  info / clear / help\n");
    }
    else if (cmd[0] != '\0') {
        current_color = 0x0C;
        print("Noux Error: Command not recognized -> ");
        print(cmd);
        print("\n");
        current_color = 0x0F;
    }
}

void keyboard_handler() {
    if (inb(0x64) & 1) {
        uint8_t scancode = inb(0x60);
        if (!(scancode & 0x80)) {
            char c = kbd_layout[scancode];
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
    (void)magic;
    (void)addr;

    init_heap(); // تهيئة مخصص الذاكرة المباشرة

    clear_screen();
    current_color = 0x0A;
    print("===================================================\n");
    print("    Welcome to Noux Bare-Metal OS Kernel v1.0      \n");
    print("       Dynamic Memory Management (Heap) Ready       \n");
    print("      Developed by Nouh Zidani (Nouh/Noux)         \n");
    print("===================================================\n\n");
    
    current_color = 0x0F;
    print("Heap initialized successfully (4 MB Heap Space).\n");
    print("Type 'meminfo' or 'alloc' to test dynamic memory.\n\n");

    print("nouh@NouxCore_BareMetal:~$ ");

    while (1) {
        keyboard_handler();
    }
}
