#include<iostream>
using namespace std;

class base
{
	public:
	virtual void foo()
	{
		cout << "This is from base class\n";
	}
};

class derived : public base
{
	public:
	void foo()
	{
		cout << "This is from derived class\n";
	}
};

class child : public derived
{
	public:
	void foo()
	{
		cout << "This is from child class\n";
	}
};	

void print(base *ptr)
{
	ptr->foo();
}

int main()
{
	//base obj;
	//obj.foo();
	base *ptr=new child;
	ptr->foo();
	base *ptr1=new derived;
	ptr1->foo();
	base b;
	derived d;
	child c;
	print(&b);
	print(&d);
	print(&c);
	
}
