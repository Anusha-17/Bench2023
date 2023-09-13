//Decimal to Binary
#include<stdio.h>
#define SIZE sizeof(int)*2
void main()
{
	int num,pos,i;
	printf("Enter the number:\n");
	scanf("%d",&num);
	for(i=SIZE-1;i>=0;i--)
	{
		pos=num>>i;
		if(pos & 1)
			printf("1");
		else
			printf("0");
	}
}
