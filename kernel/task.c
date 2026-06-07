// MyOS Simple Multitasking
// Round-robin scheduler with up to 8 tasks

#define MAX_TASKS    8
#define STACK_SIZE   4096

typedef struct {
    unsigned int esp;
    unsigned int eip;
    unsigned char *stack;
    int active;
    char name[16];
} task_t;

static task_t tasks[MAX_TASKS];
static int current_task = 0;
static int task_count = 0;
static int multitasking_enabled = 0;

extern void *malloc(unsigned int size);

void task_init() {
    for (int i = 0; i < MAX_TASKS; i++)
        tasks[i].active = 0;
    tasks[0].active = 1;
    tasks[0].name[0] = 'k'; tasks[0].name[1] = 'e';
    tasks[0].name[2] = 'r'; tasks[0].name[3] = 'n';
    tasks[0].name[4] = 'e'; tasks[0].name[5] = 'l';
    tasks[0].name[6] = 0;
    task_count = 1;
}

int task_create(void (*entry)(), const char *name) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (!tasks[i].active) {
            unsigned char *stack = (unsigned char *)malloc(STACK_SIZE);
            if (!stack) return -1;
            tasks[i].stack = stack;
            unsigned int *sp = (unsigned int *)(stack + STACK_SIZE);
            *--sp = (unsigned int)entry;
            tasks[i].esp = (unsigned int)sp;
            tasks[i].eip = (unsigned int)entry;
            tasks[i].active = 1;
            int j = 0;
            while (name[j] && j < 15) { tasks[i].name[j] = name[j]; j++; }
            tasks[i].name[j] = 0;
            task_count++;
            return i;
        }
    }
    return -1;
}

int task_get_count() { return task_count; }
int task_get_current() { return current_task; }

const char *task_get_name(int id) {
    if (id < 0 || id >= MAX_TASKS || !tasks[id].active) return 0;
    return tasks[id].name;
}

void task_kill(int id) {
    if (id <= 0 || id >= MAX_TASKS) return;
    tasks[id].active = 0;
    task_count--;
}

void task_switch() {
    if (!multitasking_enabled || task_count <= 1) return;
    int next = current_task;
    for (int i = 0; i < MAX_TASKS; i++) {
        next = (next + 1) % MAX_TASKS;
        if (tasks[next].active) break;
    }
    if (next == current_task) return;
    int prev = current_task;
    current_task = next;
    __asm__ __volatile__(
        "mov %%esp, %0\n"
        "mov %1, %%esp\n"
        : "=m"(tasks[prev].esp)
        : "m"(tasks[next].esp)
    );
}

void enable_multitasking() { multitasking_enabled = 1; }
