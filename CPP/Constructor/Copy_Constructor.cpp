#include<iostream>
using namespace std;

class cons
{
	public:
	int num;
	cons(int a)
	{
		num=a;
	}
	cons(cons &b) //copy constructor
	{
		num=b.num;
	}
};


int main()
{
	cons obj1(1);  // calling parameterized constructor
	cons obj2(obj1); // calling copy constructor
	cout << obj2.num << endl;
}
