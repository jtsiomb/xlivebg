/* dynarr - dynamic resizable C array data structure
 * author: John Tsiombikas <nuclear@member.fsf.org>
 * license: public domain
 */
#ifndef DYNARR_H_
#define DYNARR_H_

/* usage example:
 * -------------
 * int *arr = ts_dynarr_alloc(0, sizeof *arr);
 *
 * int x = 10;
 * arr = ts_dynarr_push(arr, &x);
 * x = 5;
 * arr = ts_dynarr_push(arr, &x);
 * x = 42;
 * arr = ts_dynarr_push(arr, &x);
 *
 * for(i=0; i<ts_dynarr_size(arr); i++) {
 *     printf("%d\n", arr[i]);
 *  }
 *  ts_dynarr_free(arr);
 */

void *ts_dynarr_alloc(int elem, int szelem);
void ts_dynarr_free(void *da);
void *ts_dynarr_resize(void *da, int elem);

int ts_dynarr_empty(void *da);
int ts_dynarr_size(void *da);

void *ts_dynarr_clear(void *da);

/* stack semantics */
void *ts_dynarr_push(void *da, void *item);
void *ts_dynarr_pop(void *da);


/* helper macros */
#define DYNARR_RESIZE(da, n) \
	do { (da) = ts_dynarr_resize((da), (n)); } while(0)
#define DYNARR_CLEAR(da) \
	do { (da) = ts_dynarr_clear(da); } while(0)
#define DYNARR_PUSH(da, item) \
	do { (da) = ts_dynarr_push((da), (item)); } while(0)
#define DYNARR_POP(da) \
	do { (da) = ts_dynarr_pop(da); } while(0)

/* utility macros to push characters to a string. assumes and maintains
 * the invariant that the last element is always a zero
 */
#define DYNARR_STRPUSH(da, c) \
	do { \
		char cnull = 0, ch = (char)(c); \
		(da) = ts_dynarr_pop(da); \
		(da) = ts_dynarr_push((da), &ch); \
		(da) = ts_dynarr_push((da), &cnull); \
	} while(0)

#define DYNARR_STRPOP(da) \
	do { \
		char cnull = 0; \
		(da) = ts_dynarr_pop(da); \
		(da) = ts_dynarr_pop(da); \
		(da) = ts_dynarr_push((da), &cnull); \
	} while(0)


#endif	/* DYNARR_H_ */
