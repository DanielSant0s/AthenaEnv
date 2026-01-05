/*
extern s32 CreateSema(ee_sema_t *sema);
extern s32 DeleteSema(s32 sema_id);
extern s32 SignalSema(s32 sema_id);
extern s32 iSignalSema(s32 sema_id);
extern s32 WaitSema(s32 sema_id);
extern s32 PollSema(s32 sema_id);
extern s32 iPollSema(s32 sema_id);
extern s32 ReferSemaStatus(s32 sema_id, ee_sema_t *sema);
extern s32 iReferSemaStatus(s32 sema_id, ee_sema_t *sema);
extern s32 iDeleteSema(s32 sema_id);

typedef struct t_ee_sema
{
    int count,
        max_count,
        init_count,
        wait_threads;
    u32 attr,
        option;
} ee_sema_t;

*/

#include <lockman.h>

#define MAX_MUTEXES 16

#include <kernel.h>
#include <malloc.h>
#include <stdio.h>

#ifndef EA_THFIFO
#define EA_THFIFO 0
#endif

typedef struct {
    ee_sema_t sema;
    int internal_id;
    int id;
} AthenaMutex;

AthenaMutex mutexes[MAX_MUTEXES];
int mutex_count = 0;

// Counting semaphores (non-binary) using EE semaphores directly
typedef struct {
    ee_sema_t sema;
    int internal_id;
    int id;
} AthenaSemaphore;

#define MAX_SEMAPHORES 16
static AthenaSemaphore semaphores[MAX_SEMAPHORES];

void init_lockman() {
    for(int i = 0; i < MAX_MUTEXES; i++){
        mutexes[i].id = -1;
        mutexes[i].sema.option = 0;
        mutexes[i].sema.max_count = 1;
        mutexes[i].sema.init_count = 1;
    }
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        semaphores[i].id = -1;
        semaphores[i].sema.option = 0;
        semaphores[i].sema.max_count = 0;
        semaphores[i].sema.init_count = 0;
    }
}

int create_mutex() {
    for(int i = 0; i < MAX_MUTEXES; i++){
        if (mutexes[i].id == -1){
            mutexes[i].id = i;
            mutexes[i].sema.attr = EA_THFIFO;
            mutexes[i].internal_id = CreateSema(&mutexes[i].sema);
            if (mutexes[i].internal_id < 0) {
                mutexes[i].id = -1;
                return -1;
            }
            return i;
        }
    }

    return -1;
}

void delete_mutex(int id) {
    for(int i = 0; i < MAX_MUTEXES; i++){
        if (mutexes[i].id == id){
            mutexes[i].id = -1;
            DeleteSema(mutexes[i].internal_id);
            break;
        }
    }
}

void lock_mutex(int id) {
    WaitSema(mutexes[id].internal_id);
}

void unlock_mutex(int id) {
    SignalSema(mutexes[id].internal_id);
}  

int create_semaphore(int initial_count, int max_count) {
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (semaphores[i].id == -1) {
            semaphores[i].id = i;
            semaphores[i].sema.option = 0;
            semaphores[i].sema.max_count = max_count > 0 ? max_count : 1;
            semaphores[i].sema.init_count = initial_count >= 0 ? initial_count : 0;
            semaphores[i].sema.attr = EA_THFIFO;
            semaphores[i].internal_id = CreateSema(&semaphores[i].sema);
            if (semaphores[i].internal_id < 0) {
                semaphores[i].id = -1;
                return -1;
            }
            return i;
        }
    }
    return -1;
}

void delete_semaphore(int id) {
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (semaphores[i].id == id) {
            DeleteSema(semaphores[i].internal_id);
            semaphores[i].id = -1;
            break;
        }
    }
}

void wait_semaphore(int id) {
    WaitSema(semaphores[id].internal_id);
}

void signal_semaphore(int id) {
    SignalSema(semaphores[id].internal_id);
}

