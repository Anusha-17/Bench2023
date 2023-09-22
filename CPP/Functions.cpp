#include<iostream>
using namespace std;
void myfunc()
{
	cout << "Function is executed" << endl;
}
void func(string name)
{
	cout << "Name = " << name << endl;
}
void func1(string name,int age)
{
	cout << "Name = " << name << "\t" << "Age = " << age << endl;
}
int func2(int x)
{
	return x+2;
}
void swap(int &a,int &b)
{
	int c=a;
	a=b;
	b=c;
}
void func3(int num[5])
{
	for(int i=0;i<5;i++)
	{
		cout << num[i] << "\n";
	}
} 
int main()
{
	myfunc();
	myfunc();
	myfunc();
	
	/*Function with parameter*/
	func("Anusha");
	
	/*Multiple Parameter*/
	func1("Anusha",23);
	
	/*Return value*/
	cout << func2(4) << endl;
	
	int n1=10;
	int n2=20;
	cout << "Before swap :";
	cout << n1 << "\t" << n2 << endl;
	swap(n1,n2);
	cout << "After swap :";
	cout << n1 << "\t" << n2 << endl;
	
	/*Passing Arrays*/
	int num[5]={1,2,3,4,5};
	func3(num);
	return 0;
	
}


