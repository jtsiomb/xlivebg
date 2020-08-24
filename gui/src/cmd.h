#ifndef CMD_H_
#define CMD_H_

#ifdef __cplusplus
extern "C" {
#endif

int cmd_ping(void);

int cmd_list(char *buf, int maxsz);

int cmd_getprop_str(const char *name, char *buf, int maxsz);
int cmd_getprop_int(const char *name, int *ret);
int cmd_getprop_num(const char *name, float *ret);
int cmd_getprop_vec(const char *name, float *ret);

#ifdef __cplusplus
}
#endif

#endif	/* CMD_H_ */
