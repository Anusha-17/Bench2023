/*#include<stdio.h>
void main()
{
	int i,n,sum=0;
	printf("Enter the number:\n");
	scanf("%d",&n);
	for(i=1;i<=n;i++)
	{
		sum=sum+i;
	}
	printf("Sum is = %d\n",sum);
}*/

#include<stdio.h>
void sum(int n)
{
	int i,sum=0;
	for(i=1;i<=n;i++)
	{
		sum = sum+i;
	}
	printf("Sum is = %d\n",sum);
}
void main()
{
	int n;
	printf("Enter the number:\n");
	scanf("%d",&n);
	sum(n);
}
