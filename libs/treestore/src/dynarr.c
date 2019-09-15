/* dynarr - dynamic resizable C array data structure
 * author: John Tsiombikas <nuclear@member.fsf.org>
 * license: public domain
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynarr.h"

/* The array descriptor keeps auxilliary information needed to manipulate
 * the dynamic array. It's allocated adjacent to the array buffer.
 */
struct arrdesc {
	int nelem, szelem;
	int max_elem;
	int bufsz;	/* not including the descriptor */
};

#define DESC(x)		((struct arrdesc*)((char*)(x) - sizeof(struct arrdesc)))

void *ts_dynarr_alloc(int elem, int szelem)
{
	struct arrdesc *desc;

	if(!(desc = malloc(elem * szelem + sizeof *desc))) {
		return 0;
	}
	desc->nelem = desc->max_elem = elem;
	desc->szelem = szelem;
	desc->bufsz = elem * szelem;
	return (char*)desc + sizeof *desc;
}

void ts_dynarr_free(void *da)
{
	if(da) {
		free(DESC(da));
	}
}

void *ts_dynarr_resize(void *da, int elem)
{
	int newsz;
	void *tmp;
	struct arrdesc *desc;

	if(!da) return 0;
	desc = DESC(da);

	newsz = desc->szelem * elem;

	if(!(tmp = realloc(desc, newsz + sizeof *desc))) {
		return 0;
	}
	desc = tmp;

	desc->nelem = desc->max_elem = elem;
	desc->bufsz = newsz;
	return (char*)desc + sizeof *desc;
}

int ts_dynarr_empty(void *da)
{
	return DESC(da)->nelem ? 0 : 1;
}

int ts_dynarr_size(void *da)
{
	return DESC(da)->nelem;
}


void *ts_dynarr_clear(void *da)
{
	return ts_dynarr_resize(da, 0);
}

/* stack semantics */
void *ts_dynarr_push(void *da, void *item)
{
	struct arrdesc *desc;
	int nelem;

	desc = DESC(da);
	nelem = desc->nelem;

	if(nelem >= desc->max_elem) {
		/* need to resize */
		struct arrdesc *tmp;
		int newsz = desc->max_elem ? desc->max_elem * 2 : 1;

		if(!(tmp = ts_dynarr_resize(da, newsz))) {
			fprintf(stderr, "failed to resize\n");
			return da;
		}
		da = tmp;
		desc = DESC(da);
		desc->nelem = nelem;
	}

	if(item) {
		memcpy((char*)da + desc->nelem++ * desc->szelem, item, desc->szelem);
	}
	return da;
}

void *ts_dynarr_pop(void *da)
{
	struct arrdesc *desc;
	int nelem;

	desc = DESC(da);
	nelem = desc->nelem;

	if(!nelem) return da;

	if(nelem <= desc->max_elem / 3) {
		/* reclaim space */
		struct arrdesc *tmp;
		int newsz = desc->max_elem / 2;

		if(!(tmp = ts_dynarr_resize(da, newsz))) {
			fprintf(stderr, "failed to resize\n");
			return da;
		}
		da = tmp;
		desc = DESC(da);
		desc->nelem = nelem;
	}
	desc->nelem--;

	return da;
}
