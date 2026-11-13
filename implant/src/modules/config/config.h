#ifndef CONFIG_H
#define CONFIG_H

int config_load(char *ip_out, int *port_out);

int config_save(const char *ip, int port);

int config_remove(void);

#endif