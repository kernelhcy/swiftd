#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv){
	
	struct mm_cache *mc1 = mm_cache_create("mc1", 100, 0, NULL, NULL);
	struct mm_cache *mc2 = mm_cache_create("mc2", 200, 0, NULL, NULL);
	struct mm_cache *mc3 = mm_cache_create("mc3", 300, 0, NULL, NULL);
	struct mm_cache *mc4 = mm_cache_create("mc4", 400, 0, NULL, NULL);
	struct mm_cache *mc5 = mm_cache_create("mc5", 500, 0, NULL, NULL);
	struct mm_cache *mc6 = mm_cache_create("mc6", 600, 0, NULL, NULL);
	struct mm_cache *mc7 = mm_cache_create("mc7", 700, 0, NULL, NULL);

	mm_show();

	mm_cache_destroy(mc1);
	mm_cache_destroy(mc5);
	mm_cache_destroy(mc7);

	mm_show();

	return 0;
}
