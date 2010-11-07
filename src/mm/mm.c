#include "mm.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
/*
 * The definition of struct mm_cache.
 * NOTE!!:
 * 	DO NOT modify the members unless you know what you
 * 	are doing!
 */
struct mm_cache
{
	char * name;
	size_t size;
	int align;
	void (*ctor)(void *);
	void (*dtor)(void *);

	size_t total_objs; 	//number of all the objects in this cache
	size_t total_mm; 	//total memory used by this cache

	//mm_cache is collected by a double linked list.
	struct mm_cache *pre, *next;
};

struct mm_slab
{
	int a;
};
struct mm_slabctl
{
	int a;
};

//the head of the mm_cache double linked list.
static struct mm_cache *head = NULL;

struct mm_cache * mm_cache_create(const char *name, size_t size, int align
			, void (*ctor)(void *)
			, void (*dtor)(void *))
{
	struct mm_cache *mc = malloc(sizeof(*mc));
	if(!mc){
		printf("Malloc ERROR! %s %d", __FILE__, __LINE__);
		return NULL;
	}
	mc -> name = name;
	mc -> size = size;
	mc -> align = align;
	mc -> ctor = ctor;
	mc -> dtor = dtor;

	//preppend mc to the double linked list
	if(NULL == head){
		head = mc;
		mc -> next = mc -> pre = mc;
	}
	else{
		mc -> next = head;
		mc -> pre = head -> pre;

		head -> pre -> next = mc;
		head -> pre = mc;
		
		head = mc;
	}

	return mc;
}

void mm_cache_destroy(struct mm_cache *mc)
{
	if(NULL == mc){
		return;
	}

	mc -> next -> pre = mc -> pre;
	mc -> pre -> next = mc -> next;

	if(head == mc){
		head = mc -> next;
	}

	free(mc);
}

/*
 * Alloc a object from mc.
 * return the pointer of the object.
 *
 * If an error occured, return NULL.
 */
void * mm_cache_alloc(struct mm_cache *mc)
{
	printf("Alloc an object from : %s\n", mc -> name);
	return NULL;
}

/*
 * Free a object to the memeory cache.
 *
 * @mc: the pointer of the memory cache.
 * @p: the pointer of the object.
 */
void mm_cache_free(struct mm_cache *mc, void *p)
{
	printf("Free an object to %s\n", mc -> name);

}

/*
 * Shrink the memory of ALL memory caches.
 *
 * This function will free the unused memeory of ALL memory caches
 * back to OS. Of course, it will still reserve some memory for later
 * use.
 *
 * NOTE:
 * 	DO NOT call this function unless you has no enough memory to use.
 * 	The memory caches will free the unnecessary memory by themselves.
 */
void mm_cache_shrink()
{

}

////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////
//debug functions

static void mm_cache_show(struct mm_cache *mc)
{
	if(NULL == mc){
		return;
	}

	printf("\nMemory cache name: %s\n", mc -> name);
	printf("Object size: %d  Align: %d\n", mc -> size, mc -> align);
	printf("Pre %s next %s\n", mc -> pre -> name, mc -> next -> name);
}

void mm_show()
{
	if(NULL == head){
		printf("Memory cache is empty!\n");
		return;
	}

	mm_cache_show(head);
	struct mm_cache *tmp = head -> next;
	while(tmp != NULL && tmp != head)
	{
		mm_cache_show(tmp);
		tmp = tmp -> next;
	}
}

