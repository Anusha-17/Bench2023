#include<iostream>
using namespace std;
int main()
{
	string food="pizza";
	string &meal=food; //reference to food
	cout << food << "\n";
	cout << meal << "\n";
	cout << &food << "\n"; // shows the memory address of a variable
	return 0;
}
