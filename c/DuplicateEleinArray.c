#include<stdio.h>
//void duplicatearray(int arr[],int n);
void duplicatearray(int arr[],int n)
{
	int i,j;
	for(i=0;i<n;i++)
	{
		for(j=i+1;j<n;j++)
		{
			if(arr[i]==arr[j])
			{
				printf("Duplicate element is : %d\n",arr[i]);
			}
		}
	}
}
void main()
{
	int i,arr[20],n;
	printf("Enter the size of array:\n");
	scanf("%d",&n);
	printf("Enter the elements of array:\n");
	for(i=0;i<n;i++)
	{
		scanf("%d",&arr[i]);
	}
	duplicatearray(arr,n);
}

