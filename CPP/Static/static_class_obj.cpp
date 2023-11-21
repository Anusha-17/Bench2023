#include<iostream>
using namespace std;
class sta_class
{
	int i;
	public:
	sta_class()
	{
		i=0;
		cout << "Constructor created" << endl;
	}
	~sta_class()
	{
		cout << "Destructor executed" << endl;
	}
};
int main()
{
	static sta_class obj;
	cout << "main end" << endl;
	
}
