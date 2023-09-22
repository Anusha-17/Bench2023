#include<iostream>
using namespace std;
struct student
{
	int rollno;
	string name;
}stu;
int main()
{
	struct student stu;
	cout << "Enter rollno and name : " << endl;
	cin >> stu.rollno ;
	cin >> stu.name;
	cout << "Roll No = " << stu.rollno << endl;
	cout << "Name = " << stu.name << endl;
	stu.rollno = 12;
	stu.name = "Anu";
	cout << stu.rollno << " " << stu.name << endl;
	return 0;
}
