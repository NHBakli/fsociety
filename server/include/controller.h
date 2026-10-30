#ifndef CONTROLLER_H
#define CONTROLLER_H

void handle_task_injection(int ctrl_sock, const char *buf);
void *listen_controler(void *arg);

#endif