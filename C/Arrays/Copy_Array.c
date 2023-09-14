#include<stdio.h>
#define size 100
void main()
{
	int src[size],dest[size],n,i;
	printf("Enter elements of array:\n");
	scanf("%d",&n);
	printf("Enter elements of array:\n");
	for(i=0;i<n;i++)
		scanf("%d",&src[i]);
	printf("Source array:\n");
	for(i=0;i<n;i++)
		printf("%d ",src[i]);
	for(i=0;i<n;i++)
		dest[i]=src[i];
	printf("\nDestination array:\n");
	for(i=0;i<n;i++)
		printf("%d ",dest[i]);
}
