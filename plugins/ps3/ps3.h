typedef struct {
	float x, y, z;
} vec3;

typedef struct {
	vec3 pos;
	//vec3 norm;
} vertex;

int init(void *cls);
void deinit(void *cls);
int start(long tmsec, void *cls);
void stop(void *cls);
void prop(const char *name, void *cls);
void draw(long tmsec, void *cls);
void draw_wave(long tmsec);
void perspective(float *m, float vfov, float aspect, float znear, float zfar);
