#include<iostream>
using namespace std;
int main()
{
	int a=10, b=20;
	int *const ptr=&a;
	
	cout << "Address of a =" << &a << endl;
	cout << "Value of a =" << a << endl;
	cout << "Address of ptr =" << ptr << endl;
	cout << "Value of ptr =" << *ptr << endl;
	
	*ptr=123; // We can change the value 
	
	//ptr=&b; // we cannot change the address of ptr  // error: assignment of read-only variable ‘ptr’

	cout << "Address of b =" << &b << endl; 
	cout << "Value of b =" << b << endl;
	cout << "Address of ptr =" << ptr << endl;
	cout << "Value of ptr =" << *ptr << endl;
}
