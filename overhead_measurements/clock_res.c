#include <time.h>
#include <stdio.h>

void main() {
	struct timespec resol;
	clock_getres(CLOCK_PROCESS_CPUTIME_ID, &resol);
	printf("Clock resolution: %u sec %llu ns\n", resol.tv_sec, resol.tv_nsec);
}
