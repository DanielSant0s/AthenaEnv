void init_lockman();

int create_mutex();
void delete_mutex(int id);
void lock_mutex(int id);
void unlock_mutex(int id);

// Counting semaphore abstraction (non-binary), backed by EE semaphores.
int create_semaphore(int initial_count, int max_count);
void delete_semaphore(int id);
void wait_semaphore(int id);
void signal_semaphore(int id);
