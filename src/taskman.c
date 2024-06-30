#include <kernel.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "ath_env.h"
#include "include/taskman.h"
#include "include/memory.h"
#include "include/ee_tools.h"
#include "include/dbgprintf.h"
#include "include/strUtils.h"

extern void *_gp;

/*

void update_thread_status(){
    ee_thread_status_t thread_info;
    for(int i = 0; i < tasks.size; i++){
        ReferThreadStatus(tasks.list[i]->internal_id, &thread_info);
        tasks.list[i]->status = thread_info.status;
    }
}

*/

static Task tasks[MAX_THREADS];
static int tasks_size = 0;

bool is_invalid_task(Task* task) {
    return (task->id == -1 && !task->title && task->id == -1 && task->stack_size == 0);
}

void new_task(int id, size_t stack_sz, const char* title){
    for(int i = 0; i < MAX_THREADS; i++){
        if (is_invalid_task(&tasks[i])){
            tasks[i].id = id;
            tasks[i].stack_size = stack_sz;
            tasks[i].status = 0;
            tasks[i].title = title;
            break;
        }
    }
}

s32 AthenaCreateThread(ee_thread_t *thread) {
    new_task(tasks_size++, thread->stack_size, thread->option == 0? "Unknown" : thread->option);
    return CreateThread(thread);
}

void del_task(int id){
    for(int i = 0; i < MAX_THREADS; i++){
        if (tasks[i].id == id){
            tasks[i].id = -1;
            tasks[i].stack_size = 0;
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
    tasks_size = 1;

    for (uint32_t i = &__start; i < &_end; i += sizeof(uint32_t)) {
        if(*(uint32_t*)i == (0x0C000000 + ((uint32_t)CreateThread)/4)) {
            RedirectCall(i, AthenaCreateThread);
        }
    }

    for(int i = 0; i < MAX_THREADS; i++){
        tasks[i].id = -1;
        tasks[i].stack_size = 0;
        tasks[i].status = -1;
        tasks[i].title = NULL;
    }

    ReferThreadStatus(tasks_size, &info);
    new_task(tasks_size, info.stack_size, "AthenaEnv: JavaScript Runtime");
    tasks_size++;
    ReferThreadStatus(tasks_size, &info);
    new_task(tasks_size, info.stack_size, "Kernel: Thread manager");
    tasks_size++;

    do {
        
        ReferThreadStatus(tasks_size, &info);
        if(info.stack_size != 0) {
            new_task(tasks_size, info.stack_size, "Unknown");
            tasks_size++;
        }

    } while(info.stack_size != 0); //A way to list already created threads
    
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
    thread_param.option = title;

	int thread = CreateThread(&thread_param);

	if (thread < 0) {
        free(thread_param.stack);
        return -1;
    }

    dbgprintf("%s task created.\n", title);
    
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

Task* get_tasks() {
    return &tasks;
}
