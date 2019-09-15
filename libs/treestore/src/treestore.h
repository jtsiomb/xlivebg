#ifndef TREESTORE_H_
#define TREESTORE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
#define TS_DEFVAL(x) =(x)
extern "C" {
#else
#define TS_DEFVAL(x)
#endif

/** set of user-supplied I/O functions, for ts_load_io/ts_save_io */
struct ts_io {
	void *data;

	long (*read)(void *buf, size_t bytes, void *uptr);
	long (*write)(const void *buf, size_t bytes, void *uptr);
};

enum ts_value_type { TS_STRING, TS_NUMBER, TS_VECTOR, TS_ARRAY };

/** treestore node attribute value */
struct ts_value {
	enum ts_value_type type;

	char *str;		/**< string values will have this set */
	int inum;		/**< numeric values will have this set */
	float fnum;		/**< numeric values will have this set */

	/** vector values (arrays containing ONLY numbers) will have this set */
	float *vec;		/**< elements of the vector */
	int vec_size;	/**< size of the vector (in elements), same as array_size */

	/** array values (including vectors) will have this set */
	struct ts_value *array;	/**< elements of the array */
	int array_size;			/**< size of the array (in elements) */
};

int ts_init_value(struct ts_value *tsv);
void ts_destroy_value(struct ts_value *tsv);

struct ts_value *ts_alloc_value(void);		/**< also calls ts_init_value */
void ts_free_value(struct ts_value *tsv);	/**< also calls ts_destroy_value */

/** perform a deep-copy of a ts_value */
int ts_copy_value(struct ts_value *dest, struct ts_value *src);

/** set a ts_value as a string */
int ts_set_value_str(struct ts_value *tsv, const char *str);

/** set a ts_value from a list of integers */
int ts_set_valuei_arr(struct ts_value *tsv, int count, const int *arr);
int ts_set_valueiv(struct ts_value *tsv, int count, ...);
int ts_set_valueiv_va(struct ts_value *tsv, int count, va_list ap);
int ts_set_valuei(struct ts_value *tsv, int inum);	/**< equiv: ts_set_valueiv(val, 1, inum) */

/** set a ts_value from a list of floats */
int ts_set_valuef_arr(struct ts_value *tsv, int count, const float *arr);
int ts_set_valuefv(struct ts_value *tsv, int count, ...);
int ts_set_valuefv_va(struct ts_value *tsv, int count, va_list ap);
int ts_set_valuef(struct ts_value *tsv, float fnum);	/**< equiv: ts_set_valuefv(val, 1, fnum) */

/** set a ts_value from a list of ts_value pointers. they are deep-copied as per ts_copy_value */
int ts_set_value_arr(struct ts_value *tsv, int count, const struct ts_value *arr);
int ts_set_valuev(struct ts_value *tsv, int count, ...);
int ts_set_valuev_va(struct ts_value *tsv, int count, va_list ap);


/** treestore node attribute */
struct ts_attr {
	char *name;
	struct ts_value val;

	struct ts_attr *next;
};

int ts_init_attr(struct ts_attr *attr);
void ts_destroy_attr(struct ts_attr *attr);

struct ts_attr *ts_alloc_attr(void);		/**< also calls ts_init_attr */
void ts_free_attr(struct ts_attr *attr);	/**< also calls ts_destroy_attr */

/** perform a deep-copy of a ts_attr */
int ts_copy_attr(struct ts_attr *dest, struct ts_attr *src);

int ts_set_attr_name(struct ts_attr *attr, const char *name);



/** treestore node */
struct ts_node {
	char *name;

	int attr_count;
	struct ts_attr *attr_list, *attr_tail;

	int child_count;
	struct ts_node *child_list, *child_tail;
	struct ts_node *parent;

	struct ts_node *next;	/* next sibling */
};

int ts_init_node(struct ts_node *node);
void ts_destroy_node(struct ts_node *node);

struct ts_node *ts_alloc_node(void);	/**< also calls ts_init_node */
void ts_free_node(struct ts_node *n);	/**< also calls ts_destroy_node */

/** recursively destroy all the nodes of the tree */
void ts_free_tree(struct ts_node *tree);

void ts_add_attr(struct ts_node *node, struct ts_attr *attr);
struct ts_attr *ts_get_attr(struct ts_node *node, const char *name);

const char *ts_get_attr_str(struct ts_node *node, const char *aname,
		const char *def_val TS_DEFVAL(0));
float ts_get_attr_num(struct ts_node *node, const char *aname,
		float def_val TS_DEFVAL(0.0f));
int ts_get_attr_int(struct ts_node *node, const char *aname,
		int def_val TS_DEFVAL(0.0f));
float *ts_get_attr_vec(struct ts_node *node, const char *aname,
		float *def_val TS_DEFVAL(0));
struct ts_value *ts_get_attr_array(struct ts_node *node, const char *aname,
		struct ts_value *def_val TS_DEFVAL(0));


void ts_add_child(struct ts_node *node, struct ts_node *child);
int ts_remove_child(struct ts_node *node, struct ts_node *child);
struct ts_node *ts_get_child(struct ts_node *node, const char *name);

/* load/save by opening the specified file */
struct ts_node *ts_load(const char *fname);
int ts_save(struct ts_node *tree, const char *fname);

/* load/save using the supplied FILE pointer */
struct ts_node *ts_load_file(FILE *fp);
int ts_save_file(struct ts_node *tree, FILE *fp);

/* load/save using custom I/O functions */
struct ts_node *ts_load_io(struct ts_io *io);
int ts_save_io(struct ts_node *tree, struct ts_io *io);


struct ts_attr *ts_lookup(struct ts_node *root, const char *path);
const char *ts_lookup_str(struct ts_node *root, const char *path,
		const char *def_val TS_DEFVAL(0));
float ts_lookup_num(struct ts_node *root, const char *path,
		float def_val TS_DEFVAL(0.0f));
int ts_lookup_int(struct ts_node *root, const char *path,
		int def_val TS_DEFVAL(0));
float *ts_lookup_vec(struct ts_node *root, const char *path,
		float *def_val TS_DEFVAL(0));
struct ts_value *ts_lookup_array(struct ts_node *root, const char *path,
		struct ts_value *def_val TS_DEFVAL(0));


#ifdef __cplusplus
}
#endif

#endif	/* TREESTORE_H_ */
