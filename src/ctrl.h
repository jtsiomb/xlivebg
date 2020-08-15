#ifndef CTRL_H_
#define CTRL_H_

int ctrl_init(void);
void ctrl_shutdown(void);

int *ctrl_sockets(int *count);
void ctrl_process(int s);

#endif	/* CTRL_H_ */
