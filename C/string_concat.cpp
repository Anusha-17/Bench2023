#include<iostream>
using namespace std;
int main()
{
	char s1[10] = "Hello";
	char s2[10] = "world";
	int i,j;
	for(i=0;s1[i]!='\0';i++);
	for(j=0;s2[j]!='\0';i++,j++)
		s1[i] = s2[j];
	s1[i]='\0';
	cout << s1 <<endl;

}
