//C program to check Most Significant Bit (MSB) of a number is set or not.
#include<stdio.h>
#define SIZE sizeof(int)
void main()
{
	int n,msb;
	printf("Enter the number:\n");
	scanf("%d",&n);
	msb=1 << (SIZE -1);
	if(n & msb)
		printf("MSB is set to 1\n");
	else 
		printf("MSB is set to 0\n");
}
