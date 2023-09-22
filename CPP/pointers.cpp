#include<iostream>
using namespace std;
int main()
{
	string food="pizza";
	string* ptr=&food;
	cout << food << endl;
	cout << &food << endl;
	cout << ptr << endl;
	cout << *ptr << endl;//dereference : output the value of pointer
	*ptr="burger"; // modifying value of pointer
	cout << *ptr << endl;
	cout << food << endl; //new value of food variable
	return 0;
}
