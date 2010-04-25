#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>

int main()
{
	void *lib;
	if(NULL == (lib = dlopen("/home/hcy/swiftd/dir_index.so", RTLD_NOW | RTLD_GLOBAL)))
	{
		printf("dlopen error. %s\n", dlerror());

	}
	dlclose(lib);
	return 0;
}
