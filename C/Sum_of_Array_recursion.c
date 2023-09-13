#include<stdio.h>
int sumarray(int arr[],int size)
{
	if(size==0)
	{
		return 0;
	}else
	{
		return arr[size-1] + sumarray(arr,size-1);
	}
}
int main()
{
	int arr[]={1,7,1,2,1,9,9,9};
	int size = sizeof(arr) / sizeof(arr[0]);
	int sum = sumarray(arr,size);
	printf("Sum of array elements:%d\n",sum);
	return 0;
}
