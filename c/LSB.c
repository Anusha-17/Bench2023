//C program to check Least Significant Bit (LSB) of a number is set or not.
#include<stdio.h>
void main()
{
	int n;
	printf("Enter the number:\n");
	scanf("%d",&n);
	if(n & 1)
		printf("LSB is set to 1\n");
	else
		printf("LSB is set to 0\n");
}
