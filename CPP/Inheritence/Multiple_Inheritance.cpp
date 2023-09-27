/*#include<iostream>
using namespace std;
class grandparent
{
	public:
	grandparent()
	{
		cout << "Grand Parent class" << endl;
	}
};
class parent
{
	public:
	parent()
	{
		cout << "Parent class" << endl;
	}
};	
class child : public grandparent, public parent
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
}*/


#include<iostream>
using namespace std;
class grandparent
{
	public:
	void A()
	{
		cout << "Grand Parent class" << endl;
	}
};
class parent
{
	public:
	void B()
	{
		cout << "Parent class" << endl;
	}
};	
class child : public grandparent, public parent
{
	public:
	 void c()
	{
		cout << "Child class" << endl;
	}
};
	
int main()
{
	child c_obj;
	c_obj.c();
	return 0;
}
