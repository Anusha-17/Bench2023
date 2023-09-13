#include<stdio.h>
void main()
{
	int arr[10],n,odd=0,even=0,i;
	printf("Enter the size of an array:\n");
	scanf("%d",&n);
	printf("Enter array elements:\n");
	for(i=0;i<n;i++)
	{
		scanf("%d",&arr[i]);
	}
	printf("Even numbers in an array\n");
	for(i=0;i<n;i++)
	{
		if(arr[i]%2==0)
		{
			printf("%d\t\n",arr[i]);
		}
	}
	printf("Odd numbers in an array\n");
	for(i=0;i<n;i++)
	{
		if(arr[i]%2!=0)
		{
			printf("%d\t\n",arr[i]);
		}
	}
}
