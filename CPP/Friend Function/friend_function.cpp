/*
#include<iostream>
using namespace std;
class sample
{
	int a,b;
	friend void print(sample);
};
void print(sample s) // s is explicit object
{
	s.a=10;
	s.b=20;
	cout << "a = " << s.a << endl;
	cout << "b = " << s.b << endl;
	
}
int main()
{
	sample s;
	print(s);
}
*/

#include<iostream>
using namespace std;
class test2;
class test1
{
	int a;
	public:
	void geta()
	{
		cout << "Enter a value : " << endl;
		cin >> a;
	}
	friend void big(test1,test2);
};

class test2
{
	int b;
	public:
	void getb()
	{
		cout << "Enter b value : " << endl;
		cin >> b;
	}
	friend void big(test1,test2);
};
void big(test1 t1,test2 t2)
{
	if(t1.a > t2.b)
		cout << "a is big" << endl;
	else if(t2.b > t1.a)
		cout << "b is big " << endl;
	else
		cout << "Both are equal" << endl;
}

int main()
{
	test1 t1;
	test2 t2;
	t1.geta();
	t2.getb();
	big(t1,t2);
} 
	
