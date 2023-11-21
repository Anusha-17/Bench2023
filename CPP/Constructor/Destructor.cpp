#include<iostream>
using namespace std;
class mydestructor
{
	public:
	mydestructor()
	{
		cout << "Constructor created" << endl;
	}
	~mydestructor()
	{
		cout << "Distructor executed" << endl;
	}
};

int main()
{
	mydestructor obj1;
	mydestructor obj2;
}	
