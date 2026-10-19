#include <string.h>
#include <pthread.h>
#include "../include/utils.h"

#define PORT_AGENTS   4444
#define PORT_CONTROLER 5555
#define MAX_CLIENTS   10


AgentSlot agents[MAX_CLIENTS];
pthread_mutex_t agents_mutex = PTHREAD_MUTEX_INITIALIZER;

int find_agent(const char *agent_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(agents[i].agent_id, agent_id) == 0)
            return i;
    }
    return -1;
}

int register_agent(const char *agent_id) {
    int idx = find_agent(agent_id);
    if (idx >= 0)
        return idx;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (agents[i].agent_id[0] == '\0') {
            strncpy(agents[i].agent_id, agent_id, sizeof(agents[i].agent_id) - 1);
            agents[i].has_task = 0;
            return i;
        }
    }
    return -1;
}