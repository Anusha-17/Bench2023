#include<stdio.h>
void main()
{
	int a[10],n,pos,ele,i;
	printf("Enter the number of elements:\n");
	scanf("%d",&n);
	printf("Enter array elements:\n");
	for(i=0;i<n;i++)
		scanf("%d",&a[i]);
	printf("Enter position :\n");
	scanf("%d",&pos);
	if(pos < 0 || pos >= n)
	{
		printf("Invalid position\n");
	}
	else
	{
		for(i=pos;i<n-1;i++)
			a[i]=a[i+1];
	}
	n--;
	printf("Updated array:\n");
	for(i=0;i<n;i++)
		printf("%d ",a[i]);
	printf("\n");
}
