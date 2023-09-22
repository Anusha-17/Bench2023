/*#include<iostream>
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
}*/

#include<iostream>
using namespace std;
int add(int a,int b)
{
	return a+b;
}
double add(double a,double b)
{
	return a+b;
}
int main()
{
	int n1 = add(2,3);
	double n2 = add(2.3,3.4);
	cout << "Int: " << n1 << "\n";
	cout << "Double: " << n2 << "\n";
	return 0;
}


