#include<iostream>
using namespace std;
int main()
{
	int a=10, b=20;
	int const *ptr;
	ptr=&a;
	
	cout << "Address of a = " << &a << endl;
	cout << "Value of a = " << a << endl;
	cout << "Address of ptr = " << ptr << endl;
	cout << "Value of ptr = " << *ptr << endl;
	
	ptr=&b;  // we can change the address of the pointer
	
	cout << "Address of b = " << &b << endl;
	cout << "Value of b = " << b << endl;
	cout << "Address of ptr = " << ptr << endl;
	cout << "Value of ptr = " << *ptr << endl;
	
	*ptr=124;  // error: assignment of read-only location ‘* ptr’

	cout << "Address of ptr = " << ptr << endl;
	cout << "Value of ptr = " << *ptr << endl;
	
	
}
