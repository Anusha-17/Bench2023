#include<iostream>
using namespace std;
int main()
{
	/*Concatenation*/
	string fname="Anusha";
	string lname="Jupally";
	string fullname = fname + " " + lname;
	cout << fullname << endl;
	
	/*Numbers and strings*/
	string x = "10";
	string y = "10";
	string z = x + y; // z will be a string i.e 1010
	cout << z << endl;
	
	/*String length*/
	string txt = "My name is Anusha";
	cout << "The length of the string is: " << txt.length() << endl;
	cout << "The length of the string is: " << txt.size() << endl;
	
	/*Accessing string*/
	string s="Anusha";
	cout << s[0] << endl;
	cout << s[1] << endl;
	s[2]='i';
	cout << s << endl;
	
	/*Special characters*/
	string t="I am at \"Thundersoft\" ";
	cout << t << endl;
	string t1="The character \\ is called backslash";
	cout << t1 << endl;
	string t2 = "Hello\nWorld!";
	cout << t2 << endl;
	string t3 = "Hello\tWorld!";
	cout << t3 << endl;
	
	/*User input strings*/
	string name;
	cout << "Enter your name: " << endl;
	cin >> name ;
	cout << "My name is : " << name;
	return 0;
}
