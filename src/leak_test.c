#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <string.h>
#include <unistd.h>

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
	while(1) {
		p = foo();
		++count;
		printf("%d: %p\n", count, p);
		sleep(3);
	}
	return 0;
}
