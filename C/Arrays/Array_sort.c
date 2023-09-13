#include<stdio.h>
#include<limits.h>
void main()
{
	int arr[10],n,i,j,temp;
	printf("Enter the number of elements:\n");
	scanf("%d",&n);
	printf("Enter array elements:\n");
	for(i=0;i<n;i++)
		scanf("%d",&arr[i]);
	for(i=0;i<n;i++)
	{
		for(j=i+1;j<n;j++)
		{
			if(arr[i]>arr[j])
			{
				temp=arr[i];
				arr[i]=arr[j];
				arr[j]=temp;
			}
		}
	}
	printf("Sorted elements:\n");
	for(i=0;i<n;i++)
		printf("%d ",arr[i]);
	printf("Second largest element :%d",arr[n-2]);
}
