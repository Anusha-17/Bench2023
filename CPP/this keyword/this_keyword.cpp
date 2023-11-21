#include<iostream>
using namespace std;

class A
{
	private:
		int a;
	public:
	void test(int a)
	{
		this->a=a;
	}
	void display()
	{
		cout << a << endl;
	}
};

int main()
{
	A obj;
	int a=10;
	obj.test(a);
	obj.display();
}
