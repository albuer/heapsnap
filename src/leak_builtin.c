#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <string.h>
#include <unistd.h>

#include "libheapsnap.h"

void* foo(void)
{
	void* p = malloc(4096);
	memset(p, 0x5A, 4096);
	return p;
}

int main(void)
{
	int count=0;
	void * p = NULL;

	printf("My PID: %d\n\n", getpid());
	system("chmod 0777 /data/local/tmp");

	while(1) {
		p = foo();
		++count;
		printf("%d: %p\n", count, p);
        if (count%3 == 0) {
            printf("Save heap info\n");
            heapsnap_save();
        }
		sleep(3);
	}
	return 0;
}
