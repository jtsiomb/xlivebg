#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "treestore.h"

#ifdef WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

struct ts_node *ts_text_load(struct ts_io *io);
int ts_text_save(struct ts_node *tree, struct ts_io *io);

static long io_read(void *buf, size_t bytes, void *uptr);
static long io_write(const void *buf, size_t bytes, void *uptr);


/* ---- ts_value implementation ---- */

int ts_init_value(struct ts_value *tsv)
{
	memset(tsv, 0, sizeof *tsv);
	return 0;
}

void ts_destroy_value(struct ts_value *tsv)
{
	int i;

	free(tsv->str);
	free(tsv->vec);

	for(i=0; i<tsv->array_size; i++) {
		ts_destroy_value(tsv->array + i);
	}
	free(tsv->array);
}


struct ts_value *ts_alloc_value(void)
{
	struct ts_value *v = malloc(sizeof *v);
	if(!v || ts_init_value(v) == -1) {
		free(v);
		return 0;
	}
	return v;
}

void ts_free_value(struct ts_value *tsv)
{
	ts_destroy_value(tsv);
	free(tsv);
}


int ts_copy_value(struct ts_value *dest, struct ts_value *src)
{
	int i;

	if(dest == src) return 0;

	*dest = *src;

	dest->str = 0;
	dest->vec = 0;
	dest->array = 0;

	if(src->str) {
		if(!(dest->str = malloc(strlen(src->str) + 1))) {
			goto fail;
		}
		strcpy(dest->str, src->str);
	}
	if(src->vec && src->vec_size > 0) {
		if(!(dest->vec = malloc(src->vec_size * sizeof *src->vec))) {
			goto fail;
		}
		memcpy(dest->vec, src->vec, src->vec_size * sizeof *src->vec);
	}
	if(src->array && src->array_size > 0) {
		if(!(dest->array = calloc(src->array_size, sizeof *src->array))) {
			goto fail;
		}
		for(i=0; i<src->array_size; i++) {
			if(ts_copy_value(dest->array + i, src->array + i) == -1) {
				goto fail;
			}
		}
	}
	return 0;

fail:
	free(dest->str);
	free(dest->vec);
	if(dest->array) {
		for(i=0; i<dest->array_size; i++) {
			ts_destroy_value(dest->array + i);
		}
		free(dest->array);
	}
	return -1;
}

#define MAKE_NUMSTR_FUNC(type, fmt) \
	static char *make_##type##str(type x) \
	{ \
		static char scrap[128]; \
		char *str; \
		int sz = snprintf(scrap, sizeof scrap, fmt, x); \
		if(!(str = malloc(sz + 1))) return 0; \
		sprintf(str, fmt, x); \
		return str; \
	}

MAKE_NUMSTR_FUNC(int, "%d")
MAKE_NUMSTR_FUNC(float, "%g")


struct val_list_node {
	struct ts_value val;
	struct val_list_node *next;
};

int ts_set_value_str(struct ts_value *tsv, const char *str)
{
	if(tsv->str) {
		ts_destroy_value(tsv);
		if(ts_init_value(tsv) == -1) {
			return -1;
		}
	}

	tsv->type = TS_STRING;
	if(!(tsv->str = malloc(strlen(str) + 1))) {
		return -1;
	}
	strcpy(tsv->str, str);

#if 0
	/* try to parse the string and see if it fits any of the value types */
	if(*str == '[' || *str == '{') {
		/* try to parse as a vector */
		struct val_list_node *list = 0, *tail = 0, *node;
		int nelem = 0;
		char endsym = *str++ + 2;	/* ']' is '[' + 2 and '}' is '{' + 2 */

		while(*str && *str != endsym) {
			float val = strtod(str, &endp);
			if(endp == str || !(node = malloc(sizeof *node))) {
				break;
			}
			ts_init_value(&node->val);
			ts_set_valuef(&node->val, val);
			node->next = 0;

			if(list) {
				tail->next = node;
				tail = node;
			} else {
				list = tail = node;
			}
			++nelem;
			str = endp;
		}

		if(nelem && (tsv->array = malloc(nelem * sizeof *tsv->array)) &&
				(tsv->vec = malloc(nelem * sizeof *tsv->vec))) {
			int idx = 0;
			while(list) {
				node = list;
				list = list->next;

				tsv->array[idx] = node->val;
				tsv->vec[idx] = node->val.fnum;
				++idx;
				free(node);
			}
			tsv->type = TS_VECTOR;
		}

	} else if((tsv->fnum = strtod(str, &endp)), endp != str) {
		/* it's a number I guess... */
		tsv->type = TS_NUMBER;
	}
#endif

	return 0;
}

int ts_set_valuei_arr(struct ts_value *tsv, int count, const int *arr)
{
	int i;

	if(count < 1) return -1;
	if(count == 1) {
		if(!(tsv->str = make_intstr(*arr))) {
			return -1;
		}

		tsv->type = TS_NUMBER;
		tsv->fnum = (float)*arr;
		tsv->inum = *arr;
		return 0;
	}

	/* otherwise it's an array, we need to create the ts_value array, and
	 * the simplified vector
	 */
	if(!(tsv->vec = malloc(count * sizeof *tsv->vec))) {
		return -1;
	}
	tsv->vec_size = count;

	for(i=0; i<count; i++) {
		tsv->vec[i] = arr[i];
	}

	if(!(tsv->array = malloc(count * sizeof *tsv->array))) {
		free(tsv->vec);
	}
	tsv->array_size = count;

	for(i=0; i<count; i++) {
		ts_init_value(tsv->array + i);
		ts_set_valuef(tsv->array + i, arr[i]);
	}

	tsv->type = TS_VECTOR;
	return 0;
}

int ts_set_valueiv(struct ts_value *tsv, int count, ...)
{
	int res;
	va_list ap;
	va_start(ap, count);
	res = ts_set_valueiv_va(tsv, count, ap);
	va_end(ap);
	return res;
}

int ts_set_valueiv_va(struct ts_value *tsv, int count, va_list ap)
{
	int i, *vec;

	if(count < 1) return -1;
	if(count == 1) {
		int num = va_arg(ap, int);
		ts_set_valuei(tsv, num);
		return 0;
	}

	vec = alloca(count * sizeof *vec);
	for(i=0; i<count; i++) {
		vec[i] = va_arg(ap, int);
	}
	return ts_set_valuei_arr(tsv, count, vec);
}

int ts_set_valuei(struct ts_value *tsv, int inum)
{
	return ts_set_valuei_arr(tsv, 1, &inum);
}

int ts_set_valuef_arr(struct ts_value *tsv, int count, const float *arr)
{
	int i;

	if(count < 1) return -1;
	if(count == 1) {
		if(!(tsv->str = make_floatstr(*arr))) {
			return -1;
		}

		tsv->type = TS_NUMBER;
		tsv->fnum = *arr;
		tsv->inum = (int)*arr;
		return 0;
	}

	/* otherwise it's an array, we need to create the ts_value array, and
	 * the simplified vector
	 */
	if(!(tsv->vec = malloc(count * sizeof *tsv->vec))) {
		return -1;
	}
	tsv->vec_size = count;

	for(i=0; i<count; i++) {
		tsv->vec[i] = arr[i];
	}

	if(!(tsv->array = malloc(count * sizeof *tsv->array))) {
		free(tsv->vec);
	}
	tsv->array_size = count;

	for(i=0; i<count; i++) {
		ts_init_value(tsv->array + i);
		ts_set_valuef(tsv->array + i, arr[i]);
	}

	tsv->type = TS_VECTOR;
	return 0;
}

int ts_set_valuefv(struct ts_value *tsv, int count, ...)
{
	int res;
	va_list ap;
	va_start(ap, count);
	res = ts_set_valuefv_va(tsv, count, ap);
	va_end(ap);
	return res;
}

int ts_set_valuefv_va(struct ts_value *tsv, int count, va_list ap)
{
	int i;
	float *vec;

	if(count < 1) return -1;
	if(count == 1) {
		float num = va_arg(ap, double);
		ts_set_valuef(tsv, num);
		return 0;
	}

	vec = alloca(count * sizeof *vec);
	for(i=0; i<count; i++) {
		vec[i] = va_arg(ap, double);
	}
	return ts_set_valuef_arr(tsv, count, vec);
}

int ts_set_valuef(struct ts_value *tsv, float fnum)
{
	return ts_set_valuef_arr(tsv, 1, &fnum);
}

int ts_set_value_arr(struct ts_value *tsv, int count, const struct ts_value *arr)
{
	int i, allnum = 1;

	if(count <= 1) return -1;

	if(!(tsv->array = malloc(count * sizeof *tsv->array))) {
		return -1;
	}
	tsv->array_size = count;

	for(i=0; i<count; i++) {
		if(arr[i].type != TS_NUMBER) {
			allnum = 0;
		}
		if(ts_copy_value(tsv->array + i, (struct ts_value*)arr + i) == -1) {
			while(--i >= 0) {
				ts_destroy_value(tsv->array + i);
			}
			free(tsv->array);
			tsv->array = 0;
			return -1;
		}
	}

	if(allnum) {
		if(!(tsv->vec = malloc(count * sizeof *tsv->vec))) {
			ts_destroy_value(tsv);
			return -1;
		}
		tsv->type = TS_VECTOR;
		tsv->vec_size = count;

		for(i=0; i<count; i++) {
			tsv->vec[i] = tsv->array[i].fnum;
		}
	} else {
		tsv->type = TS_ARRAY;
	}
	return 0;
}

int ts_set_valuev(struct ts_value *tsv, int count, ...)
{
	int res;
	va_list ap;
	va_start(ap, count);
	res = ts_set_valuev_va(tsv, count, ap);
	va_end(ap);
	return res;
}

int ts_set_valuev_va(struct ts_value *tsv, int count, va_list ap)
{
	int i;

	if(count <= 1) return -1;

	if(!(tsv->array = malloc(count * sizeof *tsv->array))) {
		return -1;
	}
	tsv->array_size = count;

	for(i=0; i<count; i++) {
		struct ts_value *src = va_arg(ap, struct ts_value*);
		if(ts_copy_value(tsv->array + i, src) == -1) {
			while(--i >= 0) {
				ts_destroy_value(tsv->array + i);
			}
			free(tsv->array);
			tsv->array = 0;
			return -1;
		}
	}
	return 0;
}


/* ---- ts_attr implementation ---- */

int ts_init_attr(struct ts_attr *attr)
{
	memset(attr, 0, sizeof *attr);
	return ts_init_value(&attr->val);
}

void ts_destroy_attr(struct ts_attr *attr)
{
	free(attr->name);
	ts_destroy_value(&attr->val);
}

struct ts_attr *ts_alloc_attr(void)
{
	struct ts_attr *attr = malloc(sizeof *attr);
	if(!attr || ts_init_attr(attr) == -1) {
		free(attr);
		return 0;
	}
	return attr;
}

void ts_free_attr(struct ts_attr *attr)
{
	ts_destroy_attr(attr);
	free(attr);
}

int ts_copy_attr(struct ts_attr *dest, struct ts_attr *src)
{
	if(dest == src) return 0;

	if(ts_set_attr_name(dest, src->name) == -1) {
		return -1;
	}

	if(ts_copy_value(&dest->val, &src->val) == -1) {
		ts_destroy_attr(dest);
		return -1;
	}
	return 0;
}

int ts_set_attr_name(struct ts_attr *attr, const char *name)
{
	char *n = malloc(strlen(name) + 1);
	if(!n) return -1;
	strcpy(n, name);

	free(attr->name);
	attr->name = n;
	return 0;
}


/* ---- ts_node implementation ---- */

int ts_init_node(struct ts_node *node)
{
	memset(node, 0, sizeof *node);
	return 0;
}

void ts_destroy_node(struct ts_node *node)
{
	if(!node) return;

	free(node->name);

	while(node->attr_list) {
		struct ts_attr *attr = node->attr_list;
		node->attr_list = node->attr_list->next;
		ts_free_attr(attr);
	}
}

struct ts_node *ts_alloc_node(void)
{
	struct ts_node *node = malloc(sizeof *node);
	if(!node || ts_init_node(node) == -1) {
		free(node);
		return 0;
	}
	return node;
}

void ts_free_node(struct ts_node *node)
{
	ts_destroy_node(node);
	free(node);
}

void ts_free_tree(struct ts_node *tree)
{
	if(!tree) return;

	while(tree->child_list) {
		struct ts_node *child = tree->child_list;
		tree->child_list = tree->child_list->next;
		ts_free_tree(child);
	}

	ts_free_node(tree);
}

void ts_add_attr(struct ts_node *node, struct ts_attr *attr)
{
	attr->next = 0;
	if(node->attr_list) {
		node->attr_tail->next = attr;
		node->attr_tail = attr;
	} else {
		node->attr_list = node->attr_tail = attr;
	}
	node->attr_count++;
}

struct ts_attr *ts_get_attr(struct ts_node *node, const char *name)
{
	struct ts_attr *attr = node->attr_list;
	while(attr) {
		if(strcmp(attr->name, name) == 0) {
			return attr;
		}
		attr = attr->next;
	}
	return 0;
}

const char *ts_get_attr_str(struct ts_node *node, const char *aname, const char *def_val)
{
	struct ts_attr *attr = ts_get_attr(node, aname);
	if(!attr || !attr->val.str) {
		return def_val;
	}
	return attr->val.str;
}

float ts_get_attr_num(struct ts_node *node, const char *aname, float def_val)
{
	struct ts_attr *attr = ts_get_attr(node, aname);
	if(!attr || attr->val.type != TS_NUMBER) {
		return def_val;
	}
	return attr->val.fnum;
}

int ts_get_attr_int(struct ts_node *node, const char *aname, int def_val)
{
	struct ts_attr *attr = ts_get_attr(node, aname);
	if(!attr || attr->val.type != TS_NUMBER) {
		return def_val;
	}
	return attr->val.inum;
}

float *ts_get_attr_vec(struct ts_node *node, const char *aname, float *def_val)
{
	struct ts_attr *attr = ts_get_attr(node, aname);
	if(!attr || !attr->val.vec) {
		return def_val;
	}
	return attr->val.vec;
}

struct ts_value *ts_get_attr_array(struct ts_node *node, const char *aname, struct ts_value *def_val)
{
	struct ts_attr *attr = ts_get_attr(node, aname);
	if(!attr || !attr->val.array) {
		return def_val;
	}
	return attr->val.array;
}

void ts_add_child(struct ts_node *node, struct ts_node *child)
{
	if(child->parent) {
		if(child->parent == node) return;
		ts_remove_child(child->parent, child);
	}
	child->parent = node;
	child->next = 0;

	if(node->child_list) {
		node->child_tail->next = child;
		node->child_tail = child;
	} else {
		node->child_list = node->child_tail = child;
	}
	node->child_count++;
}

int ts_remove_child(struct ts_node *node, struct ts_node *child)
{
	struct ts_node dummy, *iter = &dummy;
	dummy.next = node->child_list;

	while(iter->next && iter->next != child) {
		iter = iter->next;
	}
	if(!iter->next) {
		return -1;
	}

	child->parent = 0;

	iter->next = child->next;
	if(!iter->next) {
		node->child_tail = iter;
	}
	node->child_list = dummy.next;
	node->child_count--;
	assert(node->child_count >= 0);
	return 0;
}

struct ts_node *ts_get_child(struct ts_node *node, const char *name)
{
	struct ts_node *res = node->child_list;
	while(res) {
		if(strcmp(res->name, name) == 0) {
			return res;
		}
		res = res->next;
	}
	return 0;
}

struct ts_node *ts_load(const char *fname)
{
	FILE *fp;
	struct ts_node *root;

	if(!(fp = fopen(fname, "rb"))) {
		fprintf(stderr, "ts_load: failed to open file: %s: %s\n", fname, strerror(errno));
		return 0;
	}

	root = ts_load_file(fp);
	fclose(fp);
	return root;
}

struct ts_node *ts_load_file(FILE *fp)
{
	struct ts_io io = {0};
	io.data = fp;
	io.read = io_read;

	return ts_load_io(&io);
}

struct ts_node *ts_load_io(struct ts_io *io)
{
	return ts_text_load(io);
}

int ts_save(struct ts_node *tree, const char *fname)
{
	FILE *fp;
	int res;

	if(!(fp = fopen(fname, "wb"))) {
		fprintf(stderr, "ts_save: failed to open file: %s: %s\n", fname, strerror(errno));
		return 0;
	}
	res = ts_save_file(tree, fp);
	fclose(fp);
	return res;
}

int ts_save_file(struct ts_node *tree, FILE *fp)
{
	struct ts_io io = {0};
	io.data = fp;
	io.write = io_write;

	return ts_save_io(tree, &io);
}

int ts_save_io(struct ts_node *tree, struct ts_io *io)
{
	return ts_text_save(tree, io);
}

static const char *pathtok(const char *path, char *tok)
{
	int len;
	const char *dot = strchr(path, '.');
	if(!dot) {
		strcpy(tok, path);
		return 0;
	}

	len = dot - path;
	memcpy(tok, path, len);
	tok[len] = 0;
	return dot + 1;
}

struct ts_attr *ts_lookup(struct ts_node *node, const char *path)
{
	char *name = alloca(strlen(path) + 1);

	if(!node) return 0;

	if(!(path = pathtok(path, name)) || strcmp(name, node->name) != 0) {
		return 0;
	}

	while((path = pathtok(path, name)) && (node = ts_get_child(node, name)));

	if(path || !node) return 0;
	return ts_get_attr(node, name);
}

const char *ts_lookup_str(struct ts_node *root, const char *path, const char *def_val)
{
	struct ts_attr *attr = ts_lookup(root, path);
	if(!attr || !attr->val.str) {
		return def_val;
	}
	return attr->val.str;
}

float ts_lookup_num(struct ts_node *root, const char *path, float def_val)
{
	struct ts_attr *attr = ts_lookup(root, path);
	if(!attr || attr->val.type != TS_NUMBER) {
		return def_val;
	}
	return attr->val.fnum;
}

int ts_lookup_int(struct ts_node *root, const char *path, int def_val)
{
	struct ts_attr *attr = ts_lookup(root, path);
	if(!attr || attr->val.type != TS_NUMBER) {
		return def_val;
	}
	return attr->val.inum;
}

float *ts_lookup_vec(struct ts_node *root, const char *path, float *def_val)
{
	struct ts_attr *attr = ts_lookup(root, path);
	if(!attr || !attr->val.vec) {
		return def_val;
	}
	return attr->val.vec;
}

struct ts_value *ts_lookup_array(struct ts_node *node, const char *path, struct ts_value *def_val)
{
	struct ts_attr *attr = ts_lookup(node, path);
	if(!attr || !attr->val.array) {
		return def_val;
	}
	return attr->val.array;
}

static long io_read(void *buf, size_t bytes, void *uptr)
{
	size_t sz = fread(buf, 1, bytes, uptr);
	if(sz < bytes && errno) return -1;
	return sz;
}

static long io_write(const void *buf, size_t bytes, void *uptr)
{
	size_t sz = fwrite(buf, 1, bytes, uptr);
	if(sz < bytes && errno) return -1;
	return sz;
}
