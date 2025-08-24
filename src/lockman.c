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

typedef struct {
    ee_sema_t sema;
    int internal_id;
    int id;
} AthenaMutex;

AthenaMutex mutexes[MAX_MUTEXES];
int mutex_count = 0;

void init_lockman() {
    for(int i = 0; i < MAX_MUTEXES; i++){
        mutexes[i].id = -1;
        mutexes[i].sema.option = 0;
        mutexes[i].sema.max_count = 1;
        mutexes[i].sema.init_count = 1;
    }
}

int create_mutex() {
    for(int i = 0; i < MAX_MUTEXES; i++){
        if (mutexes[i].id == -1){
            mutexes[i].id = i;
            mutexes[i].internal_id = CreateSema(&mutexes[i].sema);
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

