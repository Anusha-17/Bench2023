#include<stdio.h>
void main()
{
	int arr[10],max,min,n,i;
	int size=sizeof(arr)/sizeof(arr[0]);
	printf("Enter number of elements:\n");
	scanf("%d",&n);
	printf("Enter array elements:\n");
	for(i=0;i<n;i++)
		scanf("%d",&arr[i]);
	max=min=arr[0];
	for(i=0;i<n;i++)
	{
		if(arr[i]>max)
			max=arr[i];
		if(arr[i]<min)
			min=arr[i];
	}
	printf("Maximum element=%d and Minimum element=%d\n",max,min);
}
