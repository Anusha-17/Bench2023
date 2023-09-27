#include<iostream>
using namespace std;
class grandparent
{
	public:
	grandparent()
	{
		cout << "Grand Parent class" << endl;
	}
};
class parent : public grandparent
{
	public:
	parent()
	{
		cout << "Parent class" << endl;
	}
};	
class child : public parent
{
	public:
	child()
	{
		cout << "Child class" << endl;
		cout << "Multilevel Inheritence" << endl;
	}
};
	
int main()
{
	child c_obj;
	return 0;
}
