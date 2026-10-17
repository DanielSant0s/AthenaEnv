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
    int id = -1;

    DIntr();
    for(int i = 0; i < MAX_MUTEXES; i++){
        if (mutexes[i].id == -1){
            mutexes[i].id = i;   // reserva o slot imediatamente
            id = i;
            break;
        }
    }
    EIntr();

    if (id == -1)
        return -1;

    mutexes[id].sema.attr = EA_THFIFO;
    mutexes[id].internal_id = CreateSema(&mutexes[id].sema);
    if (mutexes[id].internal_id < 0) {
        mutexes[id].id = -1;
        return -1;
    }

    return id;
}

void delete_mutex(int id) {
    if (id < 0 || id >= MAX_MUTEXES)
        return;

    DIntr();
    int internal_id = -1;
    if (mutexes[id].id == id) {
        internal_id = mutexes[id].internal_id;
        mutexes[id].id = -1;
    }
    EIntr();

    if (internal_id >= 0)
        DeleteSema(internal_id);
}

void lock_mutex(int id) {
    if (id < 0 || id >= MAX_MUTEXES)
        return;
    WaitSema(mutexes[id].internal_id);
}

void unlock_mutex(int id) {
    if (id < 0 || id >= MAX_MUTEXES)
        return;
    SignalSema(mutexes[id].internal_id);
}

int create_semaphore(int initial_count, int max_count) {
    int id = -1;

    DIntr();
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (semaphores[i].id == -1) {
            semaphores[i].id = i;   // reserva o slot imediatamente
            id = i;
            break;
        }
    }
    EIntr();

    if (id == -1)
        return -1;

    semaphores[id].sema.option = 0;
    semaphores[id].sema.max_count = max_count > 0 ? max_count : 1;
    semaphores[id].sema.init_count = initial_count >= 0 ? initial_count : 0;
    semaphores[id].sema.attr = EA_THFIFO;
    semaphores[id].internal_id = CreateSema(&semaphores[id].sema);
    if (semaphores[id].internal_id < 0) {
        semaphores[id].id = -1;
        return -1;
    }

    return id;
}

void delete_semaphore(int id) {
    if (id < 0 || id >= MAX_SEMAPHORES)
        return;

    DIntr();
    int internal_id = -1;
    if (semaphores[id].id == id) {
        internal_id = semaphores[id].internal_id;
        semaphores[id].id = -1;
    }
    EIntr();

    if (internal_id >= 0)
        DeleteSema(internal_id);
}

void wait_semaphore(int id) {
    if (id < 0 || id >= MAX_SEMAPHORES)
        return;
    WaitSema(semaphores[id].internal_id);
}

void signal_semaphore(int id) {
    if (id < 0 || id >= MAX_SEMAPHORES)
        return;
    SignalSema(semaphores[id].internal_id);
}