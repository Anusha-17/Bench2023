/*
#include<iostream>
using namespace std;

class myclass
{
	public:
	void mymethod()  // Inside class definition
	{
		cout << "Method Inside class definition." << endl;
	}
};


int main()
{
	myclass myobj;
	myobj.mymethod();
}
*/

/*
#include<iostream>
using namespace std;

class myclass
{
	public:
	void mymethod(); //method declaration
};

void myclass :: mymethod()
{
	cout << "Method outside class definition." << endl;
}

int main()
{
	myclass myobj;
	myobj.mymethod();
}
*/

#include<iostream>
using namespace std;

class car
{
	public:
	int speed(int maxspeed);
};

int car :: speed(int maxspeed)
{
	return maxspeed;
}

int main()
{
	car myobj;
	cout << myobj.speed(100) << endl;
}
