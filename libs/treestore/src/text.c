#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "treestore.h"
#include "dynarr.h"

struct parser {
	struct ts_io *io;
	int nline;
	char *token;
	int nextc;
};

enum { TOK_SYM, TOK_ID, TOK_NUM, TOK_STR };

static struct ts_node *read_node(struct parser *pstate);
static int read_array(struct parser *pstate, struct ts_value *tsv, char endsym);
static int next_token(struct parser *pstate);

static int print_attr(struct ts_attr *attr, struct ts_io *io, int level);
static char *value_to_str(struct ts_value *value);
static int tree_level(struct ts_node *n);
static const char *indent(int x);
static const char *toktypestr(int type);

#define EXPECT(type) \
	do { \
		if(next_token(pst) != (type)) { \
			fprintf(stderr, "expected %s token\n", toktypestr(type)); \
			goto err; \
		} \
	} while(0)

#define EXPECT_SYM(c) \
	do { \
		if(next_token(pst) != TOK_SYM || pst->token[0] != (c)) { \
			fprintf(stderr, "expected symbol: %c\n", c); \
			goto err; \
		} \
	} while(0)


struct ts_node *ts_text_load(struct ts_io *io)
{
	char *root_name;
	struct parser pstate, *pst = &pstate;
	struct ts_node *node = 0;

	pstate.io = io;
	pstate.nline = 0;
	pstate.nextc = -1;
	if(!(pstate.token = ts_dynarr_alloc(0, 1))) {
		perror("failed to allocate token string");
		return 0;
	}

	EXPECT(TOK_ID);
	if(!(root_name = strdup(pst->token))) {
		perror("failed to allocate root node name");
		ts_dynarr_free(pst->token);
		return 0;
	}
	EXPECT_SYM('{');
	if(!(node = read_node(pst))) {
		ts_dynarr_free(pst->token);
		return 0;
	}
	node->name = root_name;

err:
	ts_dynarr_free(pst->token);
	return node;
}

static int read_value(struct parser *pst, int toktype, struct ts_value *val)
{
	switch(toktype) {
	case TOK_NUM:
		ts_set_valuef(val, atof(pst->token));
		break;

	case TOK_SYM:
		if(pst->token[0] == '[' || pst->token[0] == '{') {
			char endsym = pst->token[0] + 2; /* end symbol is dist 2 from either '[' or '{' */
			if(read_array(pst, val, endsym) == -1) {
				return -1;
			}
		} else {
			fprintf(stderr, "read_node: unexpected rhs symbol: %c\n", pst->token[0]);
		}
		break;

	case TOK_ID:
	case TOK_STR:
	default:
		ts_set_value_str(val, pst->token);
	}

	return 0;
}

static struct ts_node *read_node(struct parser *pst)
{
	int type;
	struct ts_node *node;

	if(!(node = ts_alloc_node())) {
		perror("failed to allocate treestore node");
		return 0;
	}

	while((type = next_token(pst)) == TOK_ID) {
		char *id;

		if(!(id = strdup(pst->token))) {
			goto err;
		}

		EXPECT(TOK_SYM);

		if(pst->token[0] == '=') {
			/* attribute */
			struct ts_attr *attr;
			int type;

			if(!(attr = ts_alloc_attr())) {
				goto err;
			}

			if((type = next_token(pst)) == -1) {
				ts_free_attr(attr);
				fprintf(stderr, "read_node: unexpected EOF\n");
				goto err;
			}

			if(read_value(pst, type, &attr->val) == -1) {
				ts_free_attr(attr);
				fprintf(stderr, "failed to read value\n");
				goto err;
			}
			attr->name = id;
			ts_add_attr(node, attr);

		} else if(pst->token[0] == '{') {
			/* child */
			struct ts_node *child;

			if(!(child = read_node(pst))) {
				ts_free_node(node);
				return 0;
			}

			child->name = id;
			ts_add_child(node, child);

		} else {
			fprintf(stderr, "unexpected token: %s\n", pst->token);
			goto err;
		}
	}

	if(type != TOK_SYM || pst->token[0] != '}') {
		fprintf(stderr, "expected closing brace\n");
		goto err;
	}
	return node;

err:
	fprintf(stderr, "treestore read_node failed\n");
	ts_free_node(node);
	return 0;
}

static int read_array(struct parser *pst, struct ts_value *tsv, char endsym)
{
	int type;
	struct ts_value values[32];
	int i, nval = 0;
	int res;

	while((type = next_token(pst)) != -1) {
		ts_init_value(values + nval);
		if(read_value(pst, type, values + nval) == -1) {
			return -1;
		}
		if(nval < 31) {
			++nval;
		} else {
			ts_destroy_value(values + nval);
		}

		type = next_token(pst);
		if(!(type == TOK_SYM && (pst->token[0] == ',' || pst->token[0] == endsym))) {
			fprintf(stderr, "read_array: expected comma or end symbol ('%c')\n", endsym);
			return -1;
		}
		if(pst->token[0] == endsym) {
			break;	/* we're done */
		}
	}

	if(!nval) {
		return -1;
	}

	res = ts_set_value_arr(tsv, nval, values);

	for(i=0; i<nval; i++) {
		ts_destroy_value(values + i);
	}
	return res;
}

static int nextchar(struct parser *pst)
{
	char c;

	if(pst->nextc >= 0) {
		c = pst->nextc;
		pst->nextc = -1;
	} else {
		if(pst->io->read(&c, 1, pst->io->data) < 1) {
			return -1;
		}
	}
	return c;
}

static void ungetchar(char c, struct parser *pst)
{
	assert(pst->nextc == -1);
	pst->nextc = c;
}

static int next_token(struct parser *pst)
{
	int c;

	DYNARR_CLEAR(pst->token);

	/* skip whitespace */
	while((c = nextchar(pst)) != -1) {
		if(c == '#') { /* skip to end of line */
			while((c = nextchar(pst)) != -1 && c != '\n');
			if(c == -1) return -1;
		}
		if(!isspace(c)) break;
		if(c == '\n') ++pst->nline;
	}
	if(c == -1) return -1;

	DYNARR_STRPUSH(pst->token, c);

	if(isdigit(c) || c == '-' || c == '+') {
		/* token is a number */
		int found_dot = 0;
		while((c = nextchar(pst)) != -1 &&
				(isdigit(c) || (c == '.' && !found_dot))) {
			DYNARR_STRPUSH(pst->token, c);
			if(c == '.') found_dot = 1;
		}
		if(c != -1) ungetchar(c, pst);
		return TOK_NUM;
	}
	if(isalpha(c)) {
		/* token is an identifier */
		while((c = nextchar(pst)) != -1 && (isalnum(c) || c == '_')) {
			DYNARR_STRPUSH(pst->token, c);
		}
		if(c != -1) ungetchar(c, pst);
		return TOK_ID;
	}
	if(c == '"') {
		/* token is a string constant, remove the opening quote */
		DYNARR_STRPOP(pst->token);
		while((c = nextchar(pst)) != -1 && c != '"') {
			DYNARR_STRPUSH(pst->token, c);
			if(c == '\n') ++pst->nline;
		}
		if(c != '"') {
			return -1;
		}
		return TOK_STR;
	}
	return TOK_SYM;
}

int ts_text_save(struct ts_node *tree, struct ts_io *io)
{
	char *buf;
	struct ts_node *c;
	struct ts_attr *attr;
	int lvl = tree_level(tree);
	int sz, inline_attr, res = -1;

	if(!(buf = malloc(lvl + strlen(tree->name) + 4))) {
		perror("ts_text_save failed to allocate buffer");
		goto end;
	}

	if(tree->child_list || (tree->attr_list && tree->attr_list->next)) {
		inline_attr = 0;
	} else {
		inline_attr = 1;
	}

	sz = sprintf(buf, "%s%s {", indent(lvl), tree->name);
	if(!inline_attr) {
		strcat(buf, "\n");
		sz++;
	}
	if(io->write(buf, sz, io->data) < sz) {
		goto end;
	}

	attr = tree->attr_list;
	while(attr) {
		if(print_attr(attr, io, inline_attr ? -1 : lvl) == -1) {
			goto end;
		}
		attr = attr->next;
	}

	c = tree->child_list;
	while(c) {
		if(ts_text_save(c, io) == -1) {
			goto end;
		}
		c = c->next;
	}

	if(inline_attr) {
		sz = sprintf(buf, "}\n");
	} else {
		sz = sprintf(buf, "%s}\n", indent(lvl));
	}
	if(io->write(buf, sz, io->data) < sz) {
		goto end;
	}
	res = 0;
end:
	free(buf);
	return res;
}

static int print_attr(struct ts_attr *attr, struct ts_io *io, int level)
{
	char *buf, *val;
	int sz;

	if(!(val = value_to_str(&attr->val))) {
		return -1;
	}

	sz = (level >= 0 ? level : 0) + strlen(attr->name) + ts_dynarr_size(val) + 5;
	if(!(buf = malloc(sz))) {
		perror("print_attr: failed to allocate name buffer");
		ts_dynarr_free(val);
	}

	if(level >= 0) {
		sz = sprintf(buf, "%s%s = %s\n", indent(level + 1), attr->name, val);
	} else {
		sz = sprintf(buf, " %s = %s ", attr->name, val);
	}
	if(io->write(buf, sz, io->data) < sz) {
		ts_dynarr_free(val);
		free(buf);
		return -1;
	}
	ts_dynarr_free(val);
	free(buf);
	return 0;
}

static char *append_dynstr(char *dest, char *s)
{
	while(*s) {
		DYNARR_STRPUSH(dest, *s++);
	}
	return dest;
}

static char *value_to_str(struct ts_value *value)
{
	int i;
	char buf[128];
	char *str, *valstr;

	if(!(str = ts_dynarr_alloc(0, 1))) {
		return 0;
	}

	switch(value->type) {
	case TS_NUMBER:
		sprintf(buf, "%g", value->fnum);
		str = append_dynstr(str, buf);
		break;

	case TS_VECTOR:
		DYNARR_STRPUSH(str, '[');
		for(i=0; i<value->vec_size; i++) {
			if(i == 0) {
				sprintf(buf, "%g", value->vec[i]);
			} else {
				sprintf(buf, ", %g", value->vec[i]);
			}
			str = append_dynstr(str, buf);
		}
		DYNARR_STRPUSH(str, ']');
		break;

	case TS_ARRAY:
		DYNARR_STRPUSH(str, '[');
		for(i=0; i<value->array_size; i++) {
			if(i > 0) {
				str = append_dynstr(str, ", ");
			}
			if(!(valstr = value_to_str(value->array + i))) {
				ts_dynarr_free(str);
				return 0;
			}
			str = append_dynstr(str, valstr);
			ts_dynarr_free(valstr);
		}
		DYNARR_STRPUSH(str, ']');
		break;

	default:
		sprintf(buf, "\"%s\"", value->str);
		str = append_dynstr(str, buf);
	}

	return str;
}

static int tree_level(struct ts_node *n)
{
	if(!n->parent) return 0;
	return tree_level(n->parent) + 1;
}

static const char *indent(int x)
{
	static const char buf[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	const char *end = buf + sizeof buf - 1;
	return x > sizeof buf - 1 ? buf : end - x;
}

static const char *toktypestr(int type)
{
	switch(type) {
	case TOK_ID:
		return "identifier";
	case TOK_NUM:
		return "number";
	case TOK_STR:
		return "string";
	case TOK_SYM:
		return "symbol";
	}
	return "unknown";
}
