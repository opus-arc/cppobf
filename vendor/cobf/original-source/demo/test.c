/* test.c - simple test program for COBF */

#include <stdio.h>
#ifdef unix
#define MAX_COUNT 10
#else
#define MAX_COUNT 20
#endif

int main()
{
   int i;
   for (i = 0; i < MAX_COUNT; ++i)
          printf("Hello %d!\n", i);
   return 0;
}

