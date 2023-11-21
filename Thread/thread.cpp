#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#define COUNT 4
void* threadfunction(void* arg)
{
	char* data = (char*)arg;
	system(data);// /mnt/bin/camera/qcarcamtest/qcarcamcam_test -config=* data
	pthread_exit(NULL);
}
const char* mycaminfo[COUNT][70]= {
			"/mnt/bin/camera/qcarcamtest/12cam.xml",
			"/mnt/bin/camera/qcarcamtest/1cam.xml",
			"/mnt/bin/camera/qcarcamtest/8cam.xml",
			"/mnt/bin/camera/qcarcamtest/cam_id9.xml",
}; 
int main()
{
	pthread_t threads[COUNT];
	for(int i=0;i<COUNT;i++)
	{
		int result = pthread_create(&threads[i], NULL, threadfunction, (void*)mycaminfo[i]);
		if(result)
		{
			printf("Error creating thread %d\n", i);
			return -1;
		}
	}
	for(int i=0;i<COUNT;i++)
	{
		pthread_join(threads[i],NULL);
	}
	return 0;
}
