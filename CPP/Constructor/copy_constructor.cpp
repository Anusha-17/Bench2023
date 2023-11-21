/*
#include<iostream>
using namespace std;
class Example
{
	public:
	int a;
	Example(int x) // parameterized constructor
	{
		a=x;
	}
	Example(Example &obj)  // copy constructor
	{
		a=obj.a;
	}
};
int main()
{
	Example obj1(10); // calling the parameterized constructor
	Example obj2(obj1); // calling the copy constructor
	cout << obj2.a << endl;
	return 0;
}
*/

// Constructor overloading
#include<iostream>
using namespace std;
class construct
{
	public:
	float area;
	construct() //constructor with no parameter
	{
		area=0;
	}
	construct(int a,int b) //with parameter
	{
		area=a*b;
	}
	void display()
	{
		cout << area << endl;
	}
};
int main()
{
	construct obj;
	construct obj2(12,13);
	obj.display();
	obj2.display();
	return 0;
}
	
	
