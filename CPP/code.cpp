#include<iostream>
#include<string.h>
using namespace std;
const char *tok = NULL;
static char g_filename[128];
int main(int argc, char **argv)
{
	//for (int i = 1; i < argc; i++)
    	{
    	printf(" %s\n",argv[1]);
        	if (!strncmp(argv[1], "-config=", strlen("-config="))) 
        	{
        		
            		tok = argv[1] + strlen("-config=");
                        snprintf(g_filename, sizeof(g_filename), "%s", tok);   
            		printf(" %s\n",    g_filename);     		
       		 }
       	}
}
