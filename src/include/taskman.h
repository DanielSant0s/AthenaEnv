typedef struct {
    int id;
    int status;
    size_t stack_size;
    const char* title;
} Task;

bool is_invalid_task(Task* task);
void new_task(int id, size_t stack_size, const char* title);
void del_task(int id);
void init_taskman();
int create_task(const char* title, void* func, int stack_size, int priority);
void init_task(int id, void* args);
void kill_task(int id);
void exitkill_task();
Task* get_tasks();
