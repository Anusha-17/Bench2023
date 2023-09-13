/*#include<stdio.h>
void main()
{
  int a,b,sum;
  printf("Enter two numbers:\n");
  scanf("%d %d", &a,&b);
  sum=a+b;
  printf("Sum is = %d\n",sum);
}
*/


#include<stdio.h>
void sum(int a, int b)
{
   int sum = a+b;
   printf("Sum is = %d\n",sum);
}
void main()
{
  int a,b;
  printf("Enter two numbers:\n");
  scanf("%d %d", &a,&b);
  sum(a,b);
  
}
