#include<iostream>
using namespace std;
void add()
{
	cout << "Funtion without parameters" << endl ;
	cout << "Add" << endl ;
}
void add(int a,int b)
{
	a=b=10;
	cout << "Funtion with parameters" << endl ;
	cout << a+b << endl ;
}
void add(int a,int b,int c)
{
	a=b=c=10;
	cout << "Funtion with 3 parameters" << endl ;
	cout << a+b+c << endl ;
}
int main()
{
	int a,b,c;
	add();
	add(a,b);
	add(a,b,c);
	return 0;
}


