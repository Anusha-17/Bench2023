#include<iostream>
using namespace std;
int main()
{
	int x = 10;
	/*Assignment Operators*/
	x += 5;       // equivalent to x = x + y
	cout << "Sum = " << x << endl;
	x -= 5;
	cout << "Sub = " << x << endl;
	x *= 5;
	cout << "Mul = " << x << endl;
	x /= 5;
	cout << "Div = " << x << endl;
	x %= 5;
	cout << "Mod = " << x << endl;
	x &= 5;
	cout << "AND = " << x << endl;
	x |= 5;
	cout << "OR = " << x << endl;
	x ^= 5;
	cout << "Power = " << x << endl;
	
	/*Comparison Operators*/
	int a=10,b=5;
	cout << (a>b) << endl;
	cout << (a<b) << endl;
	cout << (a==b) << endl;
	cout << (a!=b) << endl;
	
	/*Logical Operator*/
	int i=5,j=3;
	cout << (i>3 && j<10) << endl;
	cout << (i>3 || j<10) << endl;
	cout << !(i>3 && j<10) << endl;
	return 0;
}
