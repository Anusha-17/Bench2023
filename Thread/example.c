#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define COUNT 1

void* threadfunction(void* arg) {
    char* data = (char*)arg;
    printf("%s", data); // Print the data before system call
    system(data);       // Execute the command
    printf("%s", data); // Print the data after system call
    pthread_exit(NULL);
}

const char* mycaminfo[COUNT] = {
    "/mnt/bin/camera/qcarcamtest/qcarcamcam_test -config=/mnt/bin/camera/qcarcam_test/cam_id9.xml"
};

int main() {
    pthread_t threads[COUNT];
    printf("Hello\n");
    for (int i = 0; i < COUNT; i++) {
        printf(" %s\n", mycaminfo[i]); // Print the command before thread creation
        int result = pthread_create(&threads[i], NULL, threadfunction, (void*)mycaminfo[i]);
        if (result) {
            printf("Error creating thread %d\n", i);
            return -1;
        }
    }
    for (int i = 0; i < COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}
