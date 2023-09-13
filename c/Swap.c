#include<stdio.h>
void main()
{
	int a,b,temp;
	printf("Enter numbers to swap\n");
	scanf("%d %d",&a,&b);
	printf("Numbers before interchange a=%d and b=%d\n", a,b);
	a=a^b; 
	b=a^b; 
	a=a^b; 
	printf("Numbers after interchange a=%d and b=%d\n", a,b);
}	
	
	
/* Swap two numbers
 temp=a;
 a=b;
 b=temp;
 
 Without 3rd variable
 a=a+b;
 b=a-b;
 a=a-b;
 
 With bitwise operators
 a=a^b;
 b=a^b;
 a=a^b;
 
 */

