#include<iostream>
using namespace std;
class value
{
	int num;
	public:
	value()
	{
		cout << "In value, Constructor without parameters" << endl ;
		num = 3;
	}
	value(int num_data)
	{
		cout << "In value, Constructor with parameter" << endl;
		num=num_data;
	}
	void display()
	{
		cout << num << endl;
	}
	
};

int main()
{
	value val_obj1; //constructor is being called
	val_obj1.display();
	
	value val_obj2(2); //constructor with parameter
	val_obj2.display();
	return 0;
}

