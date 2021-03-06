#ifndef CMD_H_
#define CMD_H_

#ifdef __cplusplus
extern "C" {
#endif

int cmd_ping(void);
int cmd_save(void);
int cmd_cfgpath(char *buf, int maxsz);

int cmd_list(char *buf, int maxsz);
int cmd_proplist(const char *bgname, char *buf, int maxsz);

int cmd_getprop_str(const char *name, char *buf, int maxsz);
int cmd_getprop_int(const char *name, int *ret);
int cmd_getprop_num(const char *name, float *ret);
int cmd_getprop_vec(const char *name, float *ret);

int cmd_setprop_str(const char *name, const char *val);
int cmd_setprop_int(const char *name, int val);
int cmd_setprop_num(const char *name, float val);
int cmd_setprop_vec(const char *name, float *val);

int cmd_rmprop(const char *name);

int cmd_getupd(long *upd_rate_usec);

#ifdef __cplusplus
}
#endif

#endif	/* CMD_H_ */
