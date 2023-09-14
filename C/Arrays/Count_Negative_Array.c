#include<stdio.h>
#define size 100
void main()
{
	int a[size],n,negative,i;
	n=sizeof(a)/sizeof(a[0]);
	printf("Enter number of elements:\n");
	scanf("%d",&n);
	printf("Enter array elements:\n");
	for(i=0;i<n;i++)
		scanf("%d",&a[i]);
	negative=0;
	for(i=0;i<n;i++)
		if(a[i]<0)
			negative++;
	printf("Total negative numbers =%d\n",negative);
}
