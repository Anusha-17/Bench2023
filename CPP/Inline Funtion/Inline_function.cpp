/*
#include<iostream>
using namespace std;
inline int cube(int c)
{
	return c*c*c;
}
int main()
{
	cout << "Cube of 3 is : " << cube(3) << endl;
}
*/

#include<iostream>
using namespace std;
class op
{
	int a,b,sum,sub;
	public:
		void get();
		void add();
		void diff();
};
inline void op ::get()
{
	cout << "Enter a and b value " << endl;
	cin >> a >> b;
}
inline void op ::add()
{
	sum=a+b;
	cout << "Sum is : " << sum << endl;
}
inline void op ::diff()
{
	sub=a-b;
	cout << "Sum is : " << sub << endl;
}
int  main()
{
	op s;
	s.get();
	s.add();
	s.diff();
}		
