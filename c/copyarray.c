#include<stdio.h>
#define SIZE 50
void copy(int[],int[],int);
void main()
{
	int src[SIZE],dest[SIZE],i,n;
	printf("Enter size of the array:\n");
	scanf("%d",&n);
	printf("Enter the elements of array:\n");
	for(i=0;i<n;i++)
	{
		scanf("%d",&src[i]);
	}
	void (*fp)(int[],int[],int)=copy;
	fp(src,dest,n);
}
void copy(int src[],int dest[],int n)
{
	int i;
	for(i=0;i<n;i++)
	{
		dest[i]=src[i];
	}
	printf("Elements of source array:\n");
	for(i=0;i<n;i++)
	{
		printf("%d\t",src[i]);
	}
	printf("Elements of destination array:\n");
	for(i=0;i<n;i++)
	{
		printf("%d\t",dest[i]);
	}
}
