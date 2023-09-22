#include<iostream>
using namespace std;
int main()
{
	string names[5]={"Anusha","Arvin","Advik","Aadhya","Arjun"};
	for(int i=0;i<5;i++)
	{
		cout << i << " = " << names[i] << "\n";
	}
	int num[5]={1,2,3,4,5};
	for(int i=0;i<5;i++)
	{
		cout << num[i] << "\n";
	}
	cout << "Size of array = " << sizeof(num) << "\n";
}
