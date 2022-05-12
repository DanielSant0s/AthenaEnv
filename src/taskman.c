#include <kernel.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "include/taskman.h"

static int running_tasks = -1;

Task** task_list;

extern void *_gp;

void new_task(int id, int internal_id, const char* title){
    task_list[id] = malloc(sizeof(Task));
    task_list[id]->id = id;
    task_list[id]->internal_id = internal_id;
    task_list[id]->title = title;
}

void init_taskman()
{
    ee_thread_status_t info;
    info.stack_size = -1;

    while(info.stack_size != 0) {
        running_tasks++;
        ReferThreadStatus(running_tasks, &info);
    } //A way to list already created threads
    
    printf("Threads running during boot: %d\n", running_tasks);
    task_list = malloc(sizeof(Task*)*running_tasks);

    new_task(0, 0, "Kernel: Splash");
    new_task(1, 1, "Main: AthenaVM");
    new_task(2, 2, "Kernel: Thread patch");

    printf("Task manager started successfully!\n");

}

int create_task(const char* title, void* func, int stack_size, int priority)
{
    Task** aux = malloc((running_tasks+1)*sizeof(Task*));
    memcpy(aux, task_list, running_tasks*sizeof(Task*));
    free(task_list);
    task_list = aux;

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

    new_task(running_tasks, thread, title);

    printf("%s task created.\n",task_list[running_tasks]->title);

    running_tasks++;
    
    return task_list[running_tasks-1]->id;

}

void init_task(int id, void* args){
    StartThread(task_list[id]->internal_id, args);
}

void kill_task(int id){
    TerminateThread(task_list[id]->internal_id);
    DeleteThread(task_list[id]->internal_id);
}

void list_tasks(){
    for(int i = 0; i < running_tasks; i++){
        printf("%d %s\n", task_list[i]->internal_id, task_list[i]->title);
    }
    ee_thread_status_t info;
    info.stack_size = -1;

    int j = -1;
    while(info.stack_size != 0) {
        j++;
        ReferThreadStatus(j, &info);
        if (info.stack_size != 0)
            printf("Thread %d stack size: %d | status: %d\n", j, info.stack_size, info.status);
    } //A way to list already created threads
   
}