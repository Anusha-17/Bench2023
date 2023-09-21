#include<iostream>
using namespace std;
class example
{
	int i=12;
	public:
	void display()
	{
		cout << i << endl;
	}
};
int main()
{
	example exl_obj;
	exl_obj.display();
	return 0;
}
