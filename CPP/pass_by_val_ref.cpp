#include<iostream>
using namespace std;
void display(int a)
{
	cout << "Pass by ref " << a << endl;
}
void display(int &a,int &b)
{
	cout << "Pass by val " << a << b << endl;
}
int main()
{
	int a=10;
	int b=12;
	display(a);
	display(a,b);
	return 0;
}
