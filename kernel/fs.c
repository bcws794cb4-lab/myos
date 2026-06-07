// MyOS Simple Filesystem
// Supports up to 32 files, 4KB each

#define MAX_FILES     32
#define MAX_FILENAME  32
#define MAX_FILESIZE  4096

typedef struct {
    char name[MAX_FILENAME];
    char data[MAX_FILESIZE];
    unsigned int size;
    int used;
} file_t;

static file_t fs[MAX_FILES];
static int fs_initialized = 0;

void fs_init() {
    for (int i = 0; i < MAX_FILES; i++)
        fs[i].used = 0;
    fs_initialized = 1;
}

// Create a new file
int fs_create(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].used) {
            // Check if name already exists
            int j = 0;
            while (name[j] && fs[i].name[j] && name[j] == fs[i].name[j]) j++;
            if (!name[j] && !fs[i].name[j]) return -1; // Already exists
        }
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].used) {
            int j = 0;
            while (name[j] && j < MAX_FILENAME - 1) {
                fs[i].name[j] = name[j];
                j++;
            }
            fs[i].name[j] = 0;
            fs[i].size = 0;
            fs[i].used = 1;
            return i;
        }
    }
    return -1; // No space
}

// Write to a file
int fs_write(const char *name, const char *data) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].used) continue;
        int j = 0;
        while (name[j] && fs[i].name[j] && name[j] == fs[i].name[j]) j++;
        if (!name[j] && !fs[i].name[j]) {
            // Found file, write data
            int k = 0;
            while (data[k] && k < MAX_FILESIZE - 1) {
                fs[i].data[k] = data[k];
                k++;
            }
            fs[i].data[k] = 0;
            fs[i].size = k;
            return k;
        }
    }
    return -1; // File not found
}

// Read from a file
const char *fs_read(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].used) continue;
        int j = 0;
        while (name[j] && fs[i].name[j] && name[j] == fs[i].name[j]) j++;
        if (!name[j] && !fs[i].name[j])
            return fs[i].data;
    }
    return 0; // Not found
}

// Delete a file
int fs_delete(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs[i].used) continue;
        int j = 0;
        while (name[j] && fs[i].name[j] && name[j] == fs[i].name[j]) j++;
        if (!name[j] && !fs[i].name[j]) {
            fs[i].used = 0;
            return 0;
        }
    }
    return -1;
}

// List all files
int fs_list(char out[][MAX_FILENAME], unsigned int sizes[], int max) {
    int count = 0;
    for (int i = 0; i < MAX_FILES && count < max; i++) {
        if (fs[i].used) {
            int j = 0;
            while (fs[i].name[j]) { out[count][j] = fs[i].name[j]; j++; }
            out[count][j] = 0;
            sizes[count] = fs[i].size;
            count++;
        }
    }
    return count;
}

int fs_file_count() {
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++)
        if (fs[i].used) count++;
    return count;
}
