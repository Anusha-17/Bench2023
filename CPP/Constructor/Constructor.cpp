/*
#include<iostream>
using namespace std;

class myclass
{
	public:
	myclass() // constructor - automatically called when an object of a class is created.
	{
		cout << "Constructor is created." << endl;
	}
};

int main()
{
	myclass myobj;
}
*/

// Constructor Parameters - Inside class
/*
#include<iostream>
#include<string.h>
using namespace std;

class myclass
{
	public:
	string name;
	int age;
	
	myclass(string a,int b)
	{
		name=a;
		age=b;
	}
};

int main()
{
	myclass myobj1("Anusha",24);
	myclass myobj2("Anu",23);
	cout << "Name = " << myobj1.name << "," << "Age = " << myobj1.age << endl;
	cout << "Name = " << myobj2.name << "," << "Age = " << myobj2.age << endl;
}
*/
 
// Constructor Parameters - outside class
#include<iostream>
#include<string.h>
using namespace std;

class myclass
{
	public:
	string name;
	int age;
	
	myclass(string a,int b);
};

myclass :: myclass(string a,int b)
{
	name=a;
	age=b;
}

int main()
{
	myclass myobj1("Anusha",24);
	myclass myobj2("Anu",23);
	cout << "Name = " << myobj1.name << "," << "Age = " << myobj1.age << endl;
	cout << "Name = " << myobj2.name << "," << "Age = " << myobj2.age << endl;
}
	
