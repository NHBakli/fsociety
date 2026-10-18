#ifndef SESSION_H
#define SESSION_H

int handle_beacon(int client_socket, const char *agent_id);
int handle_result(int client_socket, const char *agent_id, const char *result);

#endif