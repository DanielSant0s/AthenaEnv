#include <kernel.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "include/taskman.h"
#include "include/dbgprintf.h"

extern void *_gp;

/*

void update_thread_status(){
    ee_thread_status_t thread_info;
    for(int i = 0; i < tasks.size; i++){
        ReferThreadStatus(tasks.list[i]->internal_id, &thread_info);
        tasks.list[i]->status = thread_info.status;
    }
}

Tasklist* get_tasklist(){
    update_thread_status();
    return &tasks;
}

*/

static Task tasks[MAX_THREADS];
static int tasks_size = 0;

void new_task(int id, const char* title){
    for(int i = 0; i < MAX_THREADS; i++){
        if (tasks[i].id == -1 && !tasks[i].title && tasks[i].id == -1){
            tasks[i].id = id;
            tasks[i].status = 0;
            tasks[i].title = title;
            break;
        }
    }
}

void del_task(int id){
    for(int i = 0; i < MAX_THREADS; i++){
        if (tasks[i].id == id){
            tasks[i].id = -1;
            tasks[i].status = -1;
            tasks[i].title = NULL;
            break;
        }
    }

    tasks_size--;
}


void init_taskman()
{
    ee_thread_status_t info;
    info.stack_size = -1;
    tasks_size = 0;

    for(int i = 0; i < MAX_THREADS; i++){
        tasks[i].id = -1;
        tasks[i].status = -1;
        tasks[i].title = NULL;
    }

    new_task(0, "Idle");

    while(info.stack_size != 0) {
        tasks_size++;
        ReferThreadStatus(tasks_size, &info);
        new_task(tasks_size, "Main: PS2SDK/Kernel Patch");
    } //A way to list already created threads
    
    dbgprintf("Threads running during boot: %d\n", tasks_size);

    dbgprintf("Task manager started successfully!\n");

}

int create_task(const char* title, void* func, int stack_size, int priority)
{    
    ee_thread_t thread_param;
	
	thread_param.gp_reg = &_gp;
    thread_param.func = func;
    thread_param.stack_size = stack_size;
    thread_param.stack = memalign(128, stack_size);
    thread_param.initial_priority = priority;

	int thread = CreateThread(&thread_param);

	if (thread < 0) {
        free(thread_param.stack);
        return -1;
    }

    new_task(thread, title);

    printf("new Task: %d %d %s\n", thread, tasks_size, title);

    dbgprintf("%s task created.\n",tasks[tasks_size].title);

    tasks_size++;
    
    return thread;

}

void init_task(int id, void* args){
    StartThread(id, args);
}

void kill_task(int id){
    TerminateThread(id);
    DeleteThread(id);
    del_task(id);
}

void exitkill_task(){
    del_task(GetThreadId());
    ExitDeleteThread();
}
