#include<iostream>
using namespace std;

class shallow
{
	int a,b;
	int *ptr;
	public:
	shallow()
	{
		ptr=new int; //points to same memory location
	}
	void setdata(int x,int y,int z)
	{
		a=x;
		b=y;
		*ptr=z;
	}
	void display()
	{
		cout << "a = " << a << endl;
		cout << "b = " << b << endl;
		cout << "*ptr = " << *ptr << endl;
	}
};

int main()
{
	shallow sobj1;
	sobj1.setdata(1,2,3);
	shallow sobj2=sobj1;
	sobj2.display();
	cout << &sobj1.ptr << endl;
	cout << &sobj2.ptr << endl;
}
		
