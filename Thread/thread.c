#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#define COUNT 1
void* threadfunction(void* arg)
{
	char* data = (char*)arg;
	printf("%s\n",data);
	system(data);// /mnt/bin/camera/qcarcamtest/qcarcamcam_test -config=* data
	printf("%s\n",data);
	pthread_exit(NULL);
}

/*const char* mycaminfo[COUNT][70]= {
			"/mnt/bin/camera/qcarcam_test/12cam.xml",
			"/mnt/bin/camera/qcarcam_test/1cam.xml",
			"/mnt/bin/camera/qcarcam_test/8cam.xml",
			"/mnt/bin/camera/qcarcam_test/cam_id9.xml",
}; 
*/

const char(* mycaminfo)[COUNT][120]= {
			"/mnt/bin/camera/qcarcamtest/qcarcamcam_test -config=/mnt/bin/camera/qcarcam_test/cam_id9.xml"
			};
int main()
{
	pthread_t threads[COUNT];
	printf("Hello\n");
	for(int i=0;i<COUNT;i++)
	{
		printf("%s\n",mycaminfo[i]);
	        //printf(" %s\n", *mycaminfo[i]);
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
