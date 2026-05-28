
inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

int memory_strcmp(const char* str1, const char* str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) {
            return str1[i] - str2[i];
        }
        i++;
    }
    return str1[i] - str2[i];
}

void memory_clear(char* buffer, int length) {
    for (int i = 0; i < length; i++) {
        buffer[i] = '\0';
    }
}

// Pull the boundary symbol we just created in the Linker Script into C++
extern "C" unsigned int _kernel_end;

class MemoryManager {
private:
    unsigned int heap_current_address;

public:
    void init() {
        heap_current_address = (unsigned int)&_kernel_end;
        if (heap_current_address & 0xFFF) {
            heap_current_address &= 0xFFFFF000;
            heap_current_address += 0x1000;
        }
    }

    void* allocate(unsigned int size) {
        unsigned int allocated_address = heap_current_address;
        heap_current_address += size;
        if (heap_current_address & 0x3) {
            heap_current_address &= 0xFFFFFFFC;
            heap_current_address += 0x4;
        }

        return (void*)allocated_address;
    }
    unsigned int get_free_mem_start() {
        return heap_current_address;
    }
};

MemoryManager sys_memory;

struct Task {
    unsigned int esp;
    unsigned int stack[1024];
    bool active;
};

Task task_list[2];
int current_task_index = 0;

extern "C" void switch_task_context(unsigned int* old_esp, unsigned int* new_esp);

extern "C" unsigned int get_esp();

void yield() {
    int old_index = current_task_index;
    int next_index = (current_task_index + 1) % 2;
    current_task_index = next_index;
    switch_task_context(&task_list[old_index].esp, &task_list[next_index].esp);
}

void init_task(int index, void (*function_pointer)()) {
    task_list[index].active = true;
    unsigned int stack_top_index = 1023;
    task_list[index].stack[stack_top_index] = (unsigned int)function_pointer;
    task_list[index].stack[stack_top_index - 1] = 0;
    task_list[index].stack[stack_top_index - 2] = 0;
    task_list[index].stack[stack_top_index - 3] = 0;
    task_list[index].stack[stack_top_index - 4] = 0;
    task_list[index].esp = (unsigned int)&task_list[index].stack[stack_top_index - 4];
}

void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFD); 
    outb(0xA1, 0xFF);
}

struct idt_entry_struct {
    unsigned short base_low;
    unsigned short sel;
    unsigned char  always0;
    unsigned char  flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr_struct {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

idt_entry_struct idt[256];
idt_ptr_struct   idt_ptr;

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

class Terminal {
private:
    int row;
    int column;
    int start_x, start_y;
    int width, height;
    unsigned char current_color;

    unsigned short back_buffer[80 * 25]; 

public:

    void init(int sx, int sy, int w, int h, unsigned char color) {
        start_x = sx;
        start_y = sy;
        width = w;
        height = h;
        current_color = color;
        clear();
    }

    void clear() {
        unsigned short blank = ' ' | (current_color << 8);
        for (int i = 0; i < width * height; i++) {
            back_buffer[i] = blank;
        }
        row = 0;
        column = 0;
    }

    void write_char(char c) {
        if (c == '\b') {
            if (column > 0) column--;
            else if (row > 0) { row--; column = width - 1; }
            back_buffer[row * width + column] = ' ' | (current_color << 8);
            return;
        }

        if (c == '\n') {
            column = 0;
            row++;
        } else {
            back_buffer[row * width + column] = c | (current_color << 8);
            column++;
        }

        if (column >= width) {
            column = 0;
            row++;
        }

        if (row >= height) {
            for (int y = 0; y < height - 1; y++) {
                for (int x = 0; x < width; x++) {
                    back_buffer[y * width + x] = back_buffer[(y + 1) * width + x];
                }
            }
            unsigned short blank = ' ' | (current_color << 8);
            for (int x = 0; x < width; x++) {
                back_buffer[(height - 1) * width + x] = blank;
            }
            row = height - 1;
        }
    }

    void write_string(const char* str) {
        for (int i = 0; str[i] != '\0'; i++) {
            write_char(str[i]);
        }
    }

    void render() {
        volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int screen_index = (start_y + y) * 80 + (start_x + x);
                vga[screen_index] = back_buffer[y * width + x];
            }
        }
    }

    int get_global_cursor_offset() {
        return ((start_y + row) * 80) + (start_x + column);
    }
};

class Multiplexer {
private:
    Terminal terms[2];
    int active_pane;

    void update_hardware_cursor() {
        unsigned short position = terms[active_pane].get_global_cursor_offset();
        outb(0x3D4, 0x0F);
        outb(0x3D5, (unsigned char)(position & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
    }

public:
    void init() {

        terms[0].init(0, 0, 39, 25, 0x0F);

        terms[1].init(41, 0, 39, 25, 0x0A);

        active_pane = 0; 
        draw_borders();
        refresh_all();
    }

    void draw_borders() {
        volatile unsigned short* vga = (volatile unsigned short*)0xB8000;
        for (int y = 0; y < 25; y++) {
            vga[y * 80 + 39] = '|' | (0x07 << 8);
        }
    }

    void switch_focus() {
        active_pane = (active_pane + 1) % 2;
        update_hardware_cursor();
    }

    Terminal* get_pane(int index) {
        return &terms[index];
    }

    Terminal* get_active_pane() {
        return &terms[active_pane];
    }

    void refresh_all() {
        terms[0].render();
        terms[1].render();
        draw_borders();
        update_hardware_cursor();
    }
};

Multiplexer sys_mux;

class Shell {
private:
    char input_buffer[256];
    int buffer_index;
    void interpret_command() {
        sys_mux.get_active_pane()->write_string("\n");

        if (memory_strcmp(input_buffer, "help") == 0) {
            sys_mux.get_active_pane()->write_string("Available commands: help, clear, echo\n");
        } 
        else if (memory_strcmp(input_buffer, "clear") == 0) {
            sys_mux.get_active_pane()->clear();
        }
        else if (memory_strcmp(input_buffer, "meminfo") == 0) {
            sys_mux.get_active_pane()->write_string("Heap current allocation pointer: 0x");
            unsigned int addr = sys_memory.get_free_mem_start();
            char hex_str[9];
            hex_str[8] = '\0';
            const char* hex_digits = "0123456789ABCDEF";
            for(int i = 7; i >= 0; i--) {
                hex_str[i] = hex_digits[addr & 0xF];
                addr >>= 4;
            }
            sys_mux.get_active_pane()->write_string(hex_str);
            sys_mux.get_active_pane()->write_string("\n");
        } 
        else if (memory_strcmp(input_buffer, "echo") == 0) {
            sys_mux.get_active_pane()->write_string("Echo matches! (Arguments implementation requires an advanced parser).\n");
        } 
        else if (input_buffer[0] != '\0') {
            sys_mux.get_active_pane()->write_string("Unknown command: ");
            sys_mux.get_active_pane()->write_string(input_buffer);
            sys_mux.get_active_pane()->write_string("\n");
        }

        print_prompt();
    }

public:
    void init() {
        buffer_index = 0;
        memory_clear(input_buffer, 256);
        print_prompt();
    }

    void print_prompt() {
        sys_mux.get_active_pane()->write_string("kernel> ");
    }

    void handle_keystroke(char c) {
        if (c == '\n') {
            input_buffer[buffer_index] = '\0';
            interpret_command();
            buffer_index = 0;
            memory_clear(input_buffer, 256);
        } 
        else if (c == '\b') {
            if (buffer_index > 0) {
                buffer_index--;
                input_buffer[buffer_index] = '\0';
                sys_mux.get_active_pane()->write_char('\b');
            }
        } 
        else {
            if (buffer_index < 254) {
                input_buffer[buffer_index] = c;
                buffer_index++;
                sys_mux.get_active_pane()->write_char(c);
            }
        }
    }
};

Shell shell_instances[2];;
const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0,
  ' '
};
extern "C" void keyboard_handler_main() {
    unsigned char scancode = inb(0x60);
    if (scancode < 0x80) {
        char c = keyboard_map[scancode];
        if (c == '\t') {
            sys_mux.switch_focus();
        } 
        else if (c != 0) {
            int active_idx = sys_mux.get_active_pane() == sys_mux.get_pane(0) ? 0 : 1;
            shell_instances[active_idx].handle_keystroke(c);
        }
    }
    outb(0x20, 0x20);
}

extern "C" void load_idt(unsigned int);
extern "C" void keyboard_handler_wrapper();

void sample_background_task() {
    unsigned int tick_counter = 0;
    unsigned char spinner_state = 0;
    Terminal* right_pane = sys_mux.get_pane(1);
    while(1) {
        right_pane->clear();
        right_pane->write_string("=== SYSTEM LIVE METRICS ===\n\n");
        right_pane->write_string("Scheduler Status : Active\n");
        right_pane->write_string("Task Matrix Core : 2 Slots\n");
        right_pane->write_string("Execution Pulse  : [ ");
        if (spinner_state == 0)      right_pane->write_string("/");
        else if (spinner_state == 1) right_pane->write_string("-");
        else if (spinner_state == 2) right_pane->write_string("\\");
        else if (spinner_state == 3) right_pane->write_string("|");
        right_pane->write_string(" ]\n");
        spinner_state = (spinner_state + 1) % 4;
        right_pane->write_string("Cycles Processed : ");
        tick_counter++;
        unsigned int temp_ticks = tick_counter;
        char num_buffer[11];
        num_buffer[10] = '\0';
        int idx = 9;
        if (temp_ticks == 0) {
            num_buffer[idx--] = '0';
        } else {
            while (temp_ticks > 0 && idx >= 0) {
                num_buffer[idx--] = '0' + (temp_ticks % 10);
                temp_ticks /= 10;
            }
        }
        right_pane->write_string(&num_buffer[idx + 1]);
        right_pane->write_string("\n");
        sys_mux.refresh_all();
        for (volatile int d = 0; d < 6000000; d++) {}
        
        yield();
    }
}

extern "C" void kernel_main() {
    sys_memory.init();
    sys_mux.init();
    shell_instances[0].init(); 
    sys_mux.switch_focus();
    shell_instances[1].init();
    sys_mux.switch_focus();
    idt_ptr.limit = (sizeof(idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (unsigned int)&idt;
    pic_remap();
    idt_set_gate(33, (unsigned int)keyboard_handler_wrapper, 0x08, 0x8E);
    load_idt((unsigned int)&idt_ptr);
    sys_mux.get_active_pane()->write_string("Initializing Cooperative Multitasking Scheduler...\n");
    task_list[0].active = true;
    task_list[0].esp = get_esp();
    current_task_index = 0;
    init_task(1, sample_background_task);
    sys_mux.get_active_pane()->write_string("Multitasking online! Main loop running.\n");
    while(1) {
        sys_mux.refresh_all();
        yield();
    }
}