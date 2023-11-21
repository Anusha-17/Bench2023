#include<iostream>
using namespace std;

class Animal
{
	public:
	void animalsound()
	{
		cout << "The animal makes sound." << endl;
	}
};
class pig : public Animal
{
	public:
	void animalsound()
	{
		cout << "Pig makes sound wee wee." << endl;
	}
};
class cat : public Animal
{
	public:
	void animalsound()
	{
		cout << "Cat makes sound meows meows." << endl;
	}
};

int main()
{
	Animal myanimal;
	pig mypig;
	cat mycat;
	
	myanimal.animalsound();
	mypig.animalsound();
	mycat.animalsound();
}


