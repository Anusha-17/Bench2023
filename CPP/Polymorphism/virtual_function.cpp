#include<iostream>
using namespace std;

class Base_class
{
	public:
	virtual void display()
	{
		cout << "Called virtual base class function" << "\n" ;
	}
	void print()
	{
		cout << "Called base class print function" << "\n" ;
	}
};
class Child_class : public Base_class
{
	public:
	void display()
	{
		cout << "Called child class display function" << "\n" ;
	}
	void print()
	{
		cout << "Called child class print function" << "\n" ;
	}
};

int main()
{
	Base_class* base; //creating reference of base class
	Child_class child;
	base = &child;
	base->Base_class::display(); // To call virtual function
	base->print(); // To call non virtual function
	child.display();
	child.print();
	return 0;
}
