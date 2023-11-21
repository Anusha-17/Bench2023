#include<iostream>
using namespace std;

class MyClass 
{
	public:    // Public access specifier
		int x;   // Public attribute
	private:   // Private access specifier
		int y;   // Private attribute
};

int main() 
{
	MyClass myObj;
	myObj.x = 25;  // Allowed (public)
	cout << myObj.x << endl;
	myObj.y = 50;  // Not allowed (private)
	cout << myObj.y << endl;
	return 0;
}