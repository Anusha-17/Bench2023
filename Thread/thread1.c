#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
void* threadfunction(void* arg)
{
	system("./hello");
	//pthread_exit(NULL);
}
int main()
{
	pthread_t threads[2];
	for(int i=0;i<2;i++)
	{
		int result = pthread_create(&threads[i], NULL, threadfunction, (void*)i);
		if(result)
		{
			printf("Error creating thread %d\n", i);
			return -1;
		}
	}
	for(int i=0;i<2;i++)
	{
		pthread_join(threads[i],NULL);
	}
	return 0;
}
