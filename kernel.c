#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71
#define CMOS_SECONDS 0x00
#define CMOS_MINUTES 0x02
#define CMOS_HOURS 0x04
#define CMOS_DAY 0x07
#define CMOS_MONTH 0x08
#define CMOS_YEAR 0x09
#define CMOS_STATUS_A 0x0A

static inline unsigned char inb(unsigned short port) {
    unsigned char result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(unsigned short port, unsigned char data) {
    asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

static const char keymap_lower[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

static const char keymap_upper[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
};

static int cursor_x = 0;
static int cursor_y = 0;
static char* vga_buffer = (char*)VGA_MEMORY;
static int shift_pressed = 0;
static int caps_lock = 0;

#define KEY_BUFFER_SIZE 256
static char key_buffer[KEY_BUFFER_SIZE];
static int key_read_pos = 0;
static int key_write_pos = 0;

void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        vga_buffer[i] = ' ';
        vga_buffer[i + 1] = 0x07;
    }
    cursor_x = cursor_y = 0;
    update_cursor();
}

void update_cursor() {
    unsigned short pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            int pos = (cursor_y * VGA_WIDTH + cursor_x) * 2;
            vga_buffer[pos] = ' ';
        }
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;
    } else {
        int pos = (cursor_y * VGA_WIDTH + cursor_x) * 2;
        vga_buffer[pos] = c;
        vga_buffer[pos + 1] = 0x07;
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        for (int y = 0; y < VGA_HEIGHT - 1; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                int src = ((y + 1) * VGA_WIDTH + x) * 2;
                int dst = (y * VGA_WIDTH + x) * 2;
                vga_buffer[dst] = vga_buffer[src];
                vga_buffer[dst + 1] = vga_buffer[src + 1];
            }
        }
        for (int x = 0; x < VGA_WIDTH; x++) {
            int pos = ((VGA_HEIGHT - 1) * VGA_WIDTH + x) * 2;
            vga_buffer[pos] = ' ';
            vga_buffer[pos + 1] = 0x07;
        }
        cursor_y = VGA_HEIGHT - 1;
    }
    
    update_cursor();
}

void print(const char* str) {
    while (*str) putchar(*str++);
}

void keyboard_handler_main() {
    unsigned char scancode = inb(0x60);

    if (scancode & 0x80) {
        unsigned char key = scancode & 0x7F;
        if (key == 0x2A || key == 0x36) {
            shift_pressed = 0;
        }
    } else {
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
        } else if (scancode == 0x3A) {
            caps_lock = !caps_lock;
        } else {
            char character;
            if (shift_pressed ^ caps_lock) {
                character = keymap_upper[scancode];
            } else {
                character = keymap_lower[scancode];
            }
            
            if (character != 0) {
                key_buffer[key_write_pos] = character;
                key_write_pos = (key_write_pos + 1) % KEY_BUFFER_SIZE;
            }
        }
    }
    
    outb(0x20, 0x20);
}

char getchar() {
    while (key_read_pos == key_write_pos) {
        asm volatile("hlt");
    }
    
    char c = key_buffer[key_read_pos];
    key_read_pos = (key_read_pos + 1) % KEY_BUFFER_SIZE;
    return c;
}

int has_char() {
    return key_read_pos != key_write_pos;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static unsigned char bcd_to_bin(unsigned char bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void getTime() {
    unsigned char seconds, minutes, hours;
    
    asm volatile("cli");
    
    outb(CMOS_ADDRESS, CMOS_STATUS_A);
    while (inb(CMOS_DATA) & 0x80);
    
    outb(CMOS_ADDRESS, CMOS_HOURS);
    hours = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_MINUTES);
    minutes = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_SECONDS);
    seconds = inb(CMOS_DATA);
    
    asm volatile("sti");
    
    hours = bcd_to_bin(hours);
    minutes = bcd_to_bin(minutes);
    seconds = bcd_to_bin(seconds);
    
    hours = hours + 3;
    if (hours >= 24) {
        hours = hours - 24;
    }
    
    print("Current time: ");
    
    putchar((hours / 10) + '0');
    putchar((hours % 10) + '0');
    putchar(':');
    
    putchar((minutes / 10) + '0');
    putchar((minutes % 10) + '0');
    putchar(':');
    
    putchar((seconds / 10) + '0');
    putchar((seconds % 10) + '0');
    
    print(" MSK\n");
}

void getDate() {
    unsigned char day, month, year;
    
    asm volatile("cli");
    
    outb(CMOS_ADDRESS, CMOS_STATUS_A);
    while (inb(CMOS_DATA) & 0x80);
    
    outb(CMOS_ADDRESS, CMOS_DAY);
    day = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_MONTH);
    month = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_YEAR);
    year = inb(CMOS_DATA);
    
    asm volatile("sti");
    
    day = bcd_to_bin(day);
    month = bcd_to_bin(month);
    year = bcd_to_bin(year);
    
    print("Current date: ");
    
    putchar((day / 10) + '0');
    putchar((day % 10) + '0');
    putchar('.');
    
    putchar((month / 10) + '0');
    putchar((month % 10) + '0');
    putchar('.');
    
    putchar('2');
    putchar('0');
    putchar((year / 10) + '0');
    putchar((year % 10) + '0');
    
    print("\n");
}

void reboot() {
    print("Rebooting system...\n");

    for (volatile int i = 0; i < 500000; i++);
    
    unsigned char temp;
    int attempts = 0;
    const int max_attempts = 1000;

    do {
        temp = inb(0x64);
        if ((temp & 0x02) == 0) break;
        attempts++;
        for (volatile int j = 0; j < 1000; j++);
    } while (attempts < max_attempts);
    
    if (attempts >= max_attempts) {
        print("Keyboard controller not responding. Trying alternative methods...\n");
        
        outb(0x604, 0x2000);
        for (volatile int i = 0; i < 100000; i++);
        
        outb(0xB004, 0x2000);
        for (volatile int i = 0; i < 100000; i++);
        
        outb(0xCF9, 0x0E);
        for (volatile int i = 0; i < 100000; i++);
        
        print("Forcing triple fault...\n");
        asm volatile("cli");
        asm volatile("lidt 0");
        asm volatile("int $0x00");
    } else {
        outb(0x64, 0xFE);
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    print("Reboot failed! System error.\n");
    while (1) {
        asm volatile("hlt");
    }
}

void execute_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        print("Available commands:\n");
        print("  help   - show this help\n");
        print("  clear  - clear the screen\n");
        print("  fetch  - system information\n");
        print("  time   - show current time\n");
        print("  date   - show current date\n");
        print("  reboot - reboot the system\n");
    } 
    else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    }
    else if (strcmp(cmd, "time") == 0) {
        getTime();
    }
    else if (strcmp(cmd, "date") == 0) {
        getDate();
    }
    else if (strcmp(cmd, "fetch") == 0) {
        print("turbanOS v0.3 - development OS\n");
        print("author (github.com/mark-net)\n");
        print("VGA: 80x25 text mode\n");
        print("memory: 1Mb+ (core loaded at 1MB address)\n");
    }
    else if (strcmp(cmd, "reboot") == 0) {
        reboot();
    }
    else if (strcmp(cmd, "") == 0) {
    }
    else {
        print("Unknown command: '");
        print(cmd);
        print("'. Type 'help' for a list of commands.\n");
    }
}

void terminal() {
    char input[100];
    int input_pos = 0;
    
    print("> ");
    
    while (1) {
        char c = getchar();
        
        if (c == '\n') {
            putchar('\n');
            input[input_pos] = '\0';
            execute_command(input);
            input_pos = 0;
            print("> ");
        } else if (c == '\b') {
            if (input_pos > 0) {
                input_pos--;
                putchar('\b');
            }
        } else if (c >= 32 && c <= 126 && input_pos < 99) {
            input[input_pos++] = c;
            putchar(c);
        }
    }
}

void OSmain() {
    clear_screen();
    print("Booting turbanOS...\n");
    print("Type 'help' for a list of commands.\n");
    
    for (volatile int i = 0; i < 500000; i++);
    
    terminal();
}