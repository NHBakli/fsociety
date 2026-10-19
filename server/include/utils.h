#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

#define PORT_AGENTS    4444
#define PORT_CONTROLER 5555
#define MAX_CLIENTS    10

typedef struct {
    char agent_id[64];
    char task[1024];
    int  has_task;
    char result[4096];
    int  has_result;
} AgentSlot;

extern AgentSlot agents[MAX_CLIENTS];
extern pthread_mutex_t agents_mutex;

int find_agent(const char *agent_id);
int register_agent(const char *agent_id);

#endif