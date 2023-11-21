#include<iostream>
using namespace std;
int main()
{
	string name = "anusha";
	string *ptr =&name;
	cout << name << endl;
	cout << &name << endl;
	cout << ptr <<endl;
	cout << *ptr << endl;
	*ptr="anu";
	cout << *ptr << endl;
	cout << name << endl;
}
