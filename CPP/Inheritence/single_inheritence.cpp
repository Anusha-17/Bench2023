#include<iostream>
using namespace std;
class parent
{
	public:
	parent()
	{
		cout << "Parent class" << endl;
	}
};
class child : parent
{
	public:
	child()
	{
		cout << "Child class" << endl;
	}
};
	
int main()
{
	child c_obj;
	return 0;
}
