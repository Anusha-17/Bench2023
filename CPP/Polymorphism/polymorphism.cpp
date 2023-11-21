#include<iostream>
using namespace std;

class shape
{
	int i;
	int j;
	public:
	
	virtual void print()
	{
		cout << "I am in shape" << endl;
	}
	virtual int area(int len,int width)
	{
		cout << "Area is " << len*width << endl;
		return len*width;
	}
};

class circle : public shape
{
	public:
	void print()
	{
		cout << "I am in circle" << endl;
	}
	int area(int rad,int width=0)
	{
		cout << "Area circle is " << 3.14*rad*rad << endl;
		return 3.14*rad*rad;
	}
};

class rectangle : public shape
{
	public:
	void print()
	{
		cout << "I am in rectangle" << endl;
	}
	int area(int len,int width)
	{
		cout << "Area of rectangle is " << len*width << endl;
		return len*width;
	}
};
int main()
{
	shape s_obj;
	s_obj.area(5,6);
	circle c_obj;
	c_obj.area(3);
	shape *s_obj_1 = new circle;
	int area_circle = s_obj_1->area(3,2);
	shape *s_obj_2 = new rectangle;
	int area_rect = s_obj_2->area(3,5);
	s_obj_1->print();
	s_obj_2->print();
	return 0;
}

