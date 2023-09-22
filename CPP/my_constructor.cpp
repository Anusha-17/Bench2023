/*#include<iostream>
using namespace std;
class myclass
{
	public:
	myclass() //constructor
	{
		cout << "Constructor created.." << endl;
	}
};
int main()
{
	myclass myobj;  //an object of myclass (this will call the constructor)
	return 0;
}*/

/*#include<iostream>
using namespace std;
class myclass
{
	public:
	int no;
	string name;
	string place;
	myclass(int a,string b,string c) // Constructor definition inside the class with parameters
	{
		no=a;
		name=b;
		place=c;
	}
};		
int main()
{
	myclass myobj1(1,"Anusha","Hyd");
	myclass myobj2(2,"Aadhya","Hyd");
	cout << myobj1.no << " " << myobj1.name << " " << myobj1.place << endl;
	cout << myobj2.no << " " << myobj2.name << " " << myobj2.place << endl;
	return 0;
}*/

#include<iostream>
using namespace std;
class myclass
{
	public:
	int no;
	string name;
	string place;
	myclass(int a,string b,string c); 
};
myclass :: myclass(int a,string b,string c) // Constructor definition outside the class with parameters
	{
		no=a;
		name=b;
		place=c;
	}
		
int main()
{
	myclass myobj1(1,"Anusha","Hyd");
	myclass myobj2(2,"Aadhya","Hyd");
	cout << myobj1.no << " " << myobj1.name << " " << myobj1.place << endl;
	cout << myobj2.no << " " << myobj2.name << " " << myobj2.place << endl;
	return 0;
}
	
	
