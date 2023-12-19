#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#define COUNT 4

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *threadfunction(void *arg)
{
	char *data = (char*)arg;
	 pthread_mutex_lock( &mutex );
	system(data);// /mnt/bin/camera/qcarcamtest/qcarcamcam_test -config=* data
	pthread_mutex_unlock( &mutex );
	pthread_exit(NULL);
}
/*
const char* mycaminfo[COUNT]= {
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/12cam.xml",
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/1cam.xml",
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/8cam.xml",
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/cam_id9.xml",
};
*/

const char mycaminfo[COUNT][120]= {
                         //"audio_chime -g 21 /var/LRMonoPhase4"
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/cam_id9.xml",
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/1cam.xml",
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/8cam.xml",
			"/mnt/bin/camera/qcarcam_test/qcarcam_test -config=/mnt/bin/camera/qcarcam_test/12cam.xml",
};
 
int main()
{
	pthread_t threads[COUNT];
	for(int i=0;i<COUNT;i++)
	{
	        //printf("%s\n",mycaminfo[i]);
		int result = pthread_create(&threads[i], NULL, &threadfunction, (void*)&mycaminfo[i]);
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
