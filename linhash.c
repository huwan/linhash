/* linhash.c */
#include <memory.h> /* get memcmp() */
#include <string.h> /* get strdup() */
// extern void* emalloc(); /* like malloc, but calls exit() on failure */
// extern void free(void*);

/* http://www.teach.cs.toronto.edu/~ajr/270/a2/soln/emalloc.{c,h} */
#include <stdio.h>
#include <stdlib.h>
void *emalloc(unsigned size)
{
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "out of memory!\n");
        exit(1);
    }
    return p;
}

/* The `hash_x33' function unrolled four times. */
unsigned hash_x33_u4(str, len)
	register unsigned char* str;
	register int len;
{
	register unsigned int h = 0;
	for (; len >= 4; len -= 4)
	{
		h = (h << 5) + h + *str++;
		h = (h << 5) + h + *str++;
		h = (h << 5) + h + *str++;
		h = (h << 5) + h + *str++;
	}
	switch (len)
	{
		case 3:
			h = (h << 5) + h + *str++;
			/*FALLTHROUGH*/
		case 2:
			h = (h << 5) + h + *str++;
			/*FALLTHROUGH*/
		case 1:
			h = (h << 5) + h + *str++;
			break;
		default: /*case 0:*/
			break;
	}
	fprintf(stderr, "%s\n", __func__);
	return h;
}
#define HASH(str, len) hash_x33_u4((str), (len))

/* SEGMENTSIZE must be a power of 2 !! */
#define SEGMENTSIZE 256
#define DIRECTORYSIZE 256
#define DIRINDEX(address) ((address) / SEGMENTSIZE)
#define SEGINDEX(address) ((address) % SEGMENTSIZE)
#if !defined(MAXLOADFCTR)
#define MAXLOADFCTR 2
#endif /*MAXLOADFCTR*/

#if !defined(MINLOADFCTR)
#define MINLOADFCTR (MAXLOADFCTR / 2)
#endif /*MINLOADFCTR*/

typedef struct element
{
	unsigned len;
	unsigned hash;
	char* str;
	struct element* next;
} element_t;

#define new_element() (element_t*) emalloc(sizeof(element_t))

typedef struct segment
{
	element_t* elements[SEGMENTSIZE];
} segment_t;

#define new_segment() (segment_t*) emalloc(sizeof(segment_t))

typedef struct hashtable
{
	unsigned p;		/* next bucket to split */
	unsigned maxp_minus_1;	/* == maxp - 1; maxp is the upper bound */
	int slack;		/* number of insertions before expansion */
	segment_t* directory[DIRECTORYSIZE];
} hashtable_t;

/* initialize the hash table `T' */

void init_hashtable(T)
	hashtable_t* T;
{
	T->p 			= 0;
	T->maxp_minus_1 	= SEGMENTSIZE - 1;
	T->slack 		= SEGMENTSIZE * MAXLOADFCTR;
	T->directory[0] 	= new_segment();
	{	/* the first segment must be entirely cleared before used */
		element_t** p = &T->directory[0]->elements[0];
		int count = SEGMENTSIZE;
		do
		{
			*p++ = (element_t*)0;
		} while (--count > 0);
	}
	{	/* clear the rest of the directory */
		segment_t** p = &T->directory[1];
		int count = DIRECTORYSIZE - 1;
		do
		{
			*p++ = (segment_t*)0;
		} while (--count > 0);
	}
}
void expandtable(T)
	hashtable_t* T;
{
	element_t **oldbucketp, *chain, *headofold, *headofnew, *next;
	unsigned maxp0 = T->maxp_minus_1 + 1;
	unsigned newaddress = maxp0 + T->p;

	/* no more room? */
	if (newaddress >= DIRECTORYSIZE * SEGMENTSIZE)
		return; /* should allocate a larger directory */

	/* if necessary, create a new segment */
	if (SEGINDEX(newaddress) == 0)
		T->directory[DIRINDEX(newaddress)] = new_segment();

	/* locate the old (to be split) bucket */
	oldbucketp = &T->directory[DIRINDEX(T->p)]->elements[SEGINDEX(T->p)];

	/* adjust the state variables */
	if (++(T->p) > T->maxp_minus_1)
	{
		T->maxp_minus_1 = 2 * T->maxp_minus_1 + 1;
		T->p = 0;
	}
	T->slack += MAXLOADFCTR;

	/* relocate records to the new bucket (does not preserve order) */
	headofold = (element_t*)0;
	headofnew = (element_t*)0;
	for (chain = *oldbucketp; chain != (element_t*)0; chain = next)
	{
		next = chain->next;
		if (chain->hash & maxp0)
		{
			chain->next = headofnew;
			headofnew = chain;
		}
		else
		{
			chain->next = headofold;
			headofold = chain;
		}
	}
	*oldbucketp = headofold;
	T->directory[DIRINDEX(newaddress)]->elements[SEGINDEX(newaddress)]
		= headofnew;
}

/* insert string `str' with `len' characters in table `T' */

void enter(str, len, T)
	unsigned char* str;
	int len;
	hashtable_t* T;
{
	unsigned hash, address;
	element_t **chainp, *chain;

	/* locate the bucket for this string */
	hash = HASH(str, len);
	address = hash & T->maxp_minus_1;
	if (address < T->p)
		address = hash & (2 * T->maxp_minus_1 + 1);

	chainp = &T->directory[DIRINDEX(address)]->elements[SEGINDEX(address)];

	/* is the string already in the hash table? */
	for (chain = *chainp; chain != (element_t*)0; chain = chain->next)
		if (chain->hash == hash &&
			chain->len == len &&
			!memcmp(chain->str, str, len)
			)
			return;	/* already there */

	/* nope, must add new entry */
	chain = new_element();
	chain->len = len;
	chain->hash = hash;
	chain->str = strdup((char*)str);
	chain->next = *chainp;
	*chainp = chain;

	/* do we need to expand the table? */
	if (--(T->slack) < 0)
		expandtable(T);
}

void shrinktable(T)
	hashtable_t* T;
{
	segment_t* lastseg;
	element_t** chainp;
	unsigned oldlast = T->p + T->maxp_minus_1;

	if (oldlast == 0)
		return; /* cannot shrink below this */

	/* adjust the state variables */
	if (T->p == 0)
	{
		T->maxp_minus_1 >>= 1;
		T->p = T->maxp_minus_1;
	}
	else
		--(T->p);
	T->slack -= MAXLOADFCTR;

	/* insert the chain `oldlast' at the end of chain `T->p' */
	chainp = &T->directory[DIRINDEX(T->p)]->elements[SEGINDEX(T->p)];
	while (*chainp != (element_t*)0)
		chainp = &((*chainp)->next);
	lastseg = T->directory[DIRINDEX(oldlast)];
	*chainp = lastseg->elements[SEGINDEX(oldlast)];

	/* if necessary, free the last segment */
	if (SEGINDEX(oldlast) == 0)
		free((void*)lastseg);
}

/* delete string `str' with `len' characters from table `T' */

void delete (str, len, T)
	unsigned char* str;
	int len;
	hashtable_t* T;
{
	unsigned hash, address;
	element_t **prev, *here;

	/* locate the bucket for this string */
	hash = HASH(str, len);
	address = hash & T->maxp_minus_1;
	if (address < T->p)
		address = hash & (2 * T->maxp_minus_1 + 1);

	/* find the element to be removed */
	prev = &T->directory[DIRINDEX(address)]->elements[SEGINDEX(address)];
	for (; (here = *prev) != (element_t*)0; prev = &here->next)
		if (here->hash == hash &&
			here->len == len &&
			!memcmp(here->str, str, len)
			)
			break;
	if (here == (element_t*)0)
		return; /* the string wasn't there! */

	/* remove this element */
	*prev = here->next;
	free((void*)here->str);
	free((void*)here);

	/* do we need to shrink the table? the test is:
	 * keycount / currentsize < minloadfctr
	 * i.e. ((maxp+p)*maxloadfctr-slack) / (maxp+p) < minloadfctr
	 * i.e. (maxp+p)*maxloadfctr-slack < (maxp+p)*minloadfctr
	 * i.e. slack > (maxp+p)*(maxloadfctr-minloadfctr)
	 */
	if (++(T->slack) >
		(T->maxp_minus_1 + 1 + T->p) * (MAXLOADFCTR - MINLOADFCTR)
	)
		shrinktable(T);
}

int main()
{
	hashtable_t* T = (hashtable_t*) malloc(sizeof(hashtable_t));
	unsigned char *str = "hello, world";
	int len = strlen(str);

	init_hashtable(T);
	enter(str, len, T);
	delete(str, len, T);

	return 0;
}
