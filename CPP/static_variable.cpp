#include<iostream>
using namespace std;
class static_class
{
	public:
	static int i;
	static_class()
	{
		//Do nothing
	};
};
int static_class :: i=1;	
int main()
{
	static_class obj1;
	static_class obj2;
	obj1.i= 2;
	obj2.i= 3;
	cout << obj1.i << " " << obj2.i << endl;
}
