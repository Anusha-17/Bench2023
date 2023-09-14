#include<stdio.h>
#include<limits.h>
void main()
{
	int arr[10],max1,max2,n,i,j,temp;
	printf("Enter the number of elements:\n");
	scanf("%d",&n);
	printf("Enter array elements:\n");
	for(i=0;i<n;i++)
		scanf("%d",&arr[i]);
	max1=max2=0;		
	for(i=0;i<n;i++)
	{
		if(arr[i]>max1)
		{
			max2=max1;
			max1=arr[i];
		}
		else if(arr[i] > max2 && arr[i] < max1)
		{
			max2=arr[i];
		}
	}
	printf("\nlargest element is =%d\n",max1);
	printf("\nSecond largest element is =%d\n",max2);
}
	
			
