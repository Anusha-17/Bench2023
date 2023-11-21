#include<iostream>
using namespace std;
int main()
{
	int arr[5]={1,2,3,4,5};
	int (*ptr)[5];
	ptr=&arr;
	for(int i=0;i<5;i++)
		cout << *(*ptr+i) << endl;
}
