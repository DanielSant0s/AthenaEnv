#include <kernel.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "include/taskman.h"

Tasklist tasks;

extern void *_gp;

void new_task(int id, int internal_id, const char* title){
    tasks.list[id] = malloc(sizeof(Task));
    tasks.list[id]->id = id;
    tasks.list[id]->internal_id = internal_id;
    tasks.list[id]->title = title;
}

void del_task(int id){
    for(int i = 0; i < tasks.size; i++){
        if (tasks.list[i]->internal_id == id){
            Task** aux = malloc((tasks.size-1)*sizeof(Task*));
            memcpy(aux, tasks.list, i*sizeof(Task*));
            memcpy(aux+(4*i), tasks.list+(4*i), (tasks.size-i)*sizeof(Task*));
            free(tasks.list);
            tasks.list = aux;
        }
    }

    tasks.size--;

    for(int i = 0; i < tasks.size; i++){ //BAAAD, I'll rewrite it later
        tasks.list[i]->id = i;
    }
}

void init_taskman()
{
    ee_thread_status_t info;
    info.stack_size = -1;
    tasks.size = -1;

    while(info.stack_size != 0) {
        tasks.size++;
        ReferThreadStatus(tasks.size, &info);
    } //A way to list already created threads
    
    printf("Threads running during boot: %d\n", tasks.size);
    tasks.list = malloc(sizeof(Task*)*tasks.size);

    new_task(0, 0, "Kernel: Splash");
    new_task(1, 1, "Main: AthenaVM");
    new_task(2, 2, "Kernel: Thread patch");

    printf("Task manager started successfully!\n");

}

int create_task(const char* title, void* func, int stack_size, int priority)
{
    Task** aux = malloc((tasks.size+1)*sizeof(Task*));
    memcpy(aux, tasks.list, tasks.size*sizeof(Task*));
    free(tasks.list);
    tasks.list = aux;

    ee_thread_t thread_param;
	
	thread_param.gp_reg = &_gp;
    thread_param.func = func;
    thread_param.stack_size = stack_size;
    thread_param.stack = malloc(stack_size);
    thread_param.initial_priority = priority;

	int thread = CreateThread(&thread_param);

	if (thread < 0) {
        free(thread_param.stack);
    }

    new_task(tasks.size, thread, title);

    printf("%s task created.\n",tasks.list[tasks.size]->title);

    tasks.size++;
    
    return tasks.list[tasks.size-1]->id;

}

void init_task(int id, void* args){
    StartThread(tasks.list[id]->internal_id, args);
}

void kill_task(int id){
    del_task(tasks.list[id]->internal_id);
    TerminateThread(tasks.list[id]->internal_id);
    DeleteThread(tasks.list[id]->internal_id);
}

void exitkill_task(){
    del_task(GetThreadId());
    ExitDeleteThread();
}

Tasklist* get_tasklist(){
    return &tasks;
}