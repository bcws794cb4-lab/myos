// Simple heap memory manager for MyOS
// We start our heap at 1MB above our kernel

#define HEAP_START 0x200000   // 2MB mark
#define HEAP_SIZE  0x100000   // 1MB of heap

typedef struct block {
    unsigned int size;
    int free;
    struct block *next;
} block_t;

static block_t *heap = (block_t *)HEAP_START;
static int initialized = 0;

void mem_init() {
    heap->size = HEAP_SIZE - sizeof(block_t);
    heap->free = 1;
    heap->next = 0;
    initialized = 1;
}

void *malloc(unsigned int size) {
    if (!initialized) mem_init();
    if (size == 0) return 0;

    block_t *cur = heap;
    while (cur) {
        if (cur->free && cur->size >= size) {
            // Split block if big enough
            if (cur->size > size + sizeof(block_t) + 4) {
                block_t *new = (block_t *)((unsigned char *)cur + sizeof(block_t) + size);
                new->size = cur->size - size - sizeof(block_t);
                new->free = 1;
                new->next = cur->next;
                cur->next = new;
                cur->size = size;
            }
            cur->free = 0;
            return (void *)((unsigned char *)cur + sizeof(block_t));
        }
        cur = cur->next;
    }
    return 0; // Out of memory
}

void free(void *ptr) {
    if (!ptr) return;
    block_t *block = (block_t *)((unsigned char *)ptr - sizeof(block_t));
    block->free = 1;

    // Merge adjacent free blocks
    block_t *cur = heap;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(block_t) + cur->next->size;
            cur->next = cur->next->next;
        }
        cur = cur->next;
    }
}

unsigned int mem_used() {
    unsigned int used = 0;
    block_t *cur = heap;
    while (cur) {
        if (!cur->free) used += cur->size;
        cur = cur->next;
    }
    return used;
}

unsigned int mem_free() {
    unsigned int free_mem = 0;
    block_t *cur = heap;
    while (cur) {
        if (cur->free) free_mem += cur->size;
        cur = cur->next;
    }
    return free_mem;
}
