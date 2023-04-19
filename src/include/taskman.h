typedef struct Task {
    int id;
    int status;
    const char* title;
} Task;

void new_task(int id, const char* title);
void del_task(int id);
void init_taskman();
int create_task(const char* title, void* func, int stack_size, int priority);
void init_task(int id, void* args);
void kill_task(int id);
void exitkill_task();
