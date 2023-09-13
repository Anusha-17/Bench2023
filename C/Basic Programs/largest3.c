#include<stdio.h>
void main()
{
	int n1,n2,n3;
	printf("Enter three numbers:\n");
	scanf("%d %d %d", &n1,&n2,&n3);
	if(n1 >= n2 && n1 >= n3)
		printf("n1=%d is greater.\n",n1);
	if(n2 >= n1 && n2 >= n3)
		printf("n2=%d is greater.\n",n2);
	if(n3 >= n1 && n3 >= n2)
		printf("n3=%d is greater.\n",n3);
}
		
