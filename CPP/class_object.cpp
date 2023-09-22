/*#include<iostream>
using namespace std;
class example
{
	int i=12;
	public:
	void display()
	{
		cout << i << endl;
	}
};
int main()
{
	example exl_obj;
	exl_obj.display();
	return 0;
}*/


/*#include<iostream>
using namespace std;
class myclass
{
	public:
	int num;
	string name;
};
int main()
{
	myclass myobj;
	myobj.num=17;
	myobj.name="Anusha";
	cout << myobj.num << "\n";
	cout << myobj.name << "\n";
	return 0;
}*/

/*#include<iostream>
using namespace std;
class myclass
{
	public:
	int num;
	string name;
};
int main()
{
	myclass myobj1;
	myobj1.num=1;
	myobj1.name="Anusha";
	
	myclass myobj2;
	myobj2.num=2;
	myobj2.name="Aadhya";
	
	cout << myobj1.num << " " << myobj1.name << endl;
	cout << myobj2.num << " " << myobj2.name << endl;
	return 0;
}*/

/*#include<iostream>
using namespace std;
class myclass
{
	public:
	void myfun() //method/function inside the class definition
	{
		cout << "Inside My function.." << "\n";
	}
};
int main()
{
	myclass myobj;
	myobj.myfun();
	return 0;
}*/

#include<iostream>
using namespace std;
class myclass
{
	public:
	void myfun();
};
void myclass :: myfun() //method or function outside the class definition (Scope resolution operator)
{
	cout << "Outside My function.." << "\n";
}
int main()
{
	myclass myobj;
	myobj.myfun();
	return 0;
}


	
