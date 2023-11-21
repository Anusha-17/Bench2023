/*
#include<iostream>
using namespace std;
void staticdemo()
{
	static int i=0;  //Static Variable inside Functions  //These static variables are stored on static storage area , not in stack.
	cout << i << endl;
	i++;
}

int main()
{
	for(int i=0;i<5;i++)
		staticdemo();
}
*/

#include<iostream>
using namespace std;
void staticdemo()
{
	int i=0;
	cout << i << endl;
	i++;
}
int main()
{
	for(int i=0;i<5;i++)
		staticdemo();
}
