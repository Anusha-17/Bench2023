#include<iostream>
using namespace std;
int main()
{
	int time =20;
	string result = (time < 18) ? "Good day." : "Good evening.";
	cout << result << endl;
	
	int num;
	cout << "Enter the number:" << endl;
	cin >> num;
	if(num%2==0)
		cout << "Even number." << endl;
	else
		cout << "Odd number." << endl;
		
		
	int i=0;
	/*while(i<=10)
	{
		cout << i << "\n" ;
		i++;
	}*/
	
	do
	{
		cout << i << "\n" ;
		i++;
	}while(i<5);
		
}


