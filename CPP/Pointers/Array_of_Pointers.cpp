/*#include<iostream>
using namespace std;
int main()
{
	int *ptr;
	int num[5];
	cout << "Enter numbers : " << endl;
	for(int i=0;i<5;i++)
	{
		cin >> num[i];
	}
	ptr=num;
	cout << "Numbers are : " << endl;
	for(int i=0;i<5;i++)
		cout << ptr[i] << endl;
}
*/		
// Array of pointers

#include<iostream>
using namespace std;
int main()
{
	int *ptr[5]; //array of 5 integer pointer
	int arr[5];
	cout << "Enter numbers" << endl;
	for(int i=0;i<5;i++)
		cin >> arr[i];
	
	for(int i=0;i<5;i++)
		ptr[i]=&arr[i];
	
	cout << "Numbers are : " << endl;
	for(int i=0;i<5;i++)
		cout << *ptr[i] << endl;
}

