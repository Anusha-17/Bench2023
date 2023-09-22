#include<iostream>
using namespace std;
int main()
{
	int x=10, y=20;
	int choice;
	cout << "Enter your choice:" << endl;
	cin >> choice;
	switch(choice)
	{
		case 1: cout << "Sum = " << x+y << endl;
			break;
		case 2: cout << "Sub = " << x-y << endl;
			break;
		case 3: cout << "Mul = " << x*y << endl;
			break;
		case 4: cout << "Div = " << x/y << endl;
			break;
		default : cout << "Invalid choice." << endl;
	}
}
		
