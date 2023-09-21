#include<iostream>
using namespace std;
int main()
{
	int a=10;
	int b=12;
	int sum = a+b;
	cout << "Sum = " << sum << endl;
	int mul = a*b;
	cout << "Multiplication = " << mul << endl;
	
	/*int x,y,z;
	x=y=z=20;
	cout << x + y + z << endl;*/
	
	int x=1,y=2,z=3;
	cout << x + y + z << endl;
	
	/*const int n = 17;
	n=9;       // error: assignment of read-only variable ‘n’
	cout << n << endl;*/
	
	const int m = 12;
	const float PI =  3.142;
	cout << m << endl << PI << endl;
	return 0;
}


