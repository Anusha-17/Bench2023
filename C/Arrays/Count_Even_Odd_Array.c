#include<stdio.h>
#define size 100
void main()
{
	int a[size],n,even,odd,i;
	n= sizeof(a)/sizeof(a[0]);
	printf("Enter number of array elements:\n");
	scanf("%d",&n);
	printf("Enter array elements:\n");
	for(i=0;i<n;i++)
		scanf("%d",&a[i]);
	even=odd=0;
	for(i=0;i<n;i++)
	{
		if(a[i]%2==0)
			even++;
		else
			odd++;
	}
	printf("Total even numbers =%d\n",even);
	printf("Total odd numbers =%d\n",odd);
}
	
