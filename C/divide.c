#include<stdio.h>
void main()
{
	int dividend,divisor,quotient,remainder;
	printf("Enter divident: \n");
	scanf("%d",&dividend);
	printf("Enter divisor: \n");
	scanf("%d",&divisor);
	quotient=dividend / divisor;
	remainder=dividend % divisor;
	printf("Quotient =%d\n",quotient);
	printf("Remainder =%d\n",remainder);
}
	
	
