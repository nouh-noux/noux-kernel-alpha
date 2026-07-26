#include <stdint.h>
#include <stddef.h>

__asm__(
    ".section .multiboot\n"
    ".align 4\n"
    ".long 0x1BADB002\n"
    ".long 0x00000003\n"
    ".long -(0x1BADB002 + 0x00000003)\n"
    
    ".section .text\n"
    ".global _start\n"
    "_start:\n"
    "    call kernel_main\n"
    "    cli\n"
    "    hlt\n"
);

volatile uint16_t* vga_hardware_buffer = (volatile uint16_t*)0xB8000;

size_t cursor_row = 16;
size_t cursor_col = 28; 

char input_buffer[128];
size_t input_len = 0;

int shift_pressed = 0;

// الجدول العادي
const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};

// جدول الرموز والحروف الكبيرة الشامل عند ضغط Shift
const char keyboard_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' '
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void noux_print(const char* text, int row, int col, uint8_t color) {
    for (size_t i = 0; text[i] != '\0'; i++) {
        size_t index = row * 80 + (col + i);
        vga_hardware_buffer[index] = (uint16_t)text[i] | (uint16_t)color << 8;
    }
}

void noux_print_char(char c, int row, int col, uint8_t color) {
    size_t index = row * 80 + col;
    vga_hardware_buffer[index] = (uint16_t)c | (uint16_t)color << 8;
}

void noux_clear_screen() {
    for (size_t i = 0; i < 80 * 25; i++) {
        vga_hardware_buffer[i] = (uint16_t)' ' | (uint16_t)0x07 << 8;
    }
}

int noux_strncmp(const char* s1, const char* s2, size_t n) {
    while (n--) {
        if (*s1 != *s2) return *s1 - *s2;
        if (*s1 == 0) break;
        s1++; s2++;
    }
    return 0;
}

void execute_command() {
    input_buffer[input_len] = '\0'; 
    cursor_row++; 

    if (noux_strncmp(input_buffer, "gasty print ", 12) == 0) {
        noux_print("[Gasty Output]: ", cursor_row, 5, 0x0A);
        noux_print(input_buffer + 12, cursor_row, 21, 0x0F);
    } 
    else if (input_len > 0) {
        noux_print("Noux Bare-Metal: Command not found.", cursor_row, 5, 0x04); 
    }

    cursor_row++;
    noux_print("nouh@NouxCore_BareMetal:~$ ", cursor_row, 5, 0x0F);
    cursor_col = 28;
    input_len = 0; 
}

void check_keyboard() {
    // التأكد من أن منفذ لوحة المفاتيح لديه بيانات جاهزة للقراءة
    if (inb(0x64) & 1) {
        uint8_t scancode = inb(0x60);
        
        // إذا ضغطت Shift الأيسر (0x2A) أو الأيمن (0x36)
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            return;
        }
        // إذا أفلتَّ Shift الأيسر (0xAA) أو الأيمن (0xB6)
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
            return;
        }

        // معالجة الأزرار العادية عند الضغط فقط (الرموز الأصغر من 128)
        if (scancode < 128) {
            char key = shift_pressed ? keyboard_map_shift[scancode] : keyboard_map[scancode];
            
            if (key == '\n') {
                execute_command();
            } 
            else if (key == '\b') {
                if (cursor_col > 28 && input_len > 0) {
                    cursor_col--;
                    input_len--;
                    noux_print_char(' ', cursor_row, cursor_col, 0x0F);
                }
            }
            else if (key != 0 && input_len < 60) {
                input_buffer[input_len++] = key; 
                noux_print_char(key, cursor_row, cursor_col, 0x0F);
                cursor_col++;
            }

            // حل مشكلة الالتصاق: انتظر هنا طالما أن الزر لا يزال مضغوطاً ولم يفلت بعد
            while (inb(0x64) & 1) {
                inb(0x60); // استهلاك النبضات المكررة لمنع الالتصاق
            }
        }
    }
}

void kernel_main(void) {
    noux_clear_screen();

    noux_print("[ Noux Kernel v1.0 - Independent Boot ]", 1, 20, 0x0B); 
    noux_print("--------------------------------------------------", 2, 15, 0x07);
    
    noux_print("[+] CPU Protected Mode Status: ACTIVE", 5, 5, 0x0A); 
    noux_print("[+] Simulated Memory Management: INITIALIZED", 6, 5, 0x0A);
    noux_print("[+] Gasty Language Subsystem: ACTIVE (Bare-metal)", 7, 5, 0x0E); 
    noux_print("[+] Virtual Storage Inodes: READY", 8, 5, 0x0A);

    noux_print("==================================================", 11, 15, 0x07);
    noux_print("  Welcome to your pure independent environment!   ", 12, 15, 0x0F); 
    noux_print("==================================================", 13, 15, 0x07);

    noux_print("nouh@NouxCore_BareMetal:~$ ", 16, 5, 0x0F);

    while (1) {
        check_keyboard();
    }
}
