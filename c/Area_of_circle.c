#include<stdio.h>
#define pi 3.142
void main()
{
	int r;
	printf("Enter the radious: \n");
	scanf("%d",&r);
	float area = pi*r*r;
	printf("Area of circle is : %f\n", area);
}
