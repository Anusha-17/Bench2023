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
	printf("Enter value to insert:\n");
	scanf("%d",&ele);
	if(pos < 0 || pos > n)
	{
		printf("Invalid position\n");
	}
	else
	{
		for(i=n;i>pos;i--)
			a[i]=a[i-1];
	}
	a[pos]=ele;
	n++;
	printf("Updated array:\n");
	for(i=0;i<n;i++)
		printf("%d ",a[i]);
	printf("\n");
}
