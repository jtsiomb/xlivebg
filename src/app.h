#ifndef APP_H_
#define APP_H_

unsigned int bgtex;
unsigned long msec;

int app_init(int argc, char **argv);
void app_cleanup(void);

void app_draw(void);
void app_reshape(int x, int y);

void app_keyboard(int key, int pressed);

void app_quit(void);

#endif	/* APP_H_ */
