/* Unary operator overloading */
/*
#include <iostream>
using namespace std;

class OverLoad {
   private:
    int a;
    int b;

   public:
    OverLoad() : a(0), b(0) // Constructor to initialize the value of a and b to '0'
    {
    } 

    void input() 
    {
        cout << "Enter the first number : ";
        cin >> a;
        cout<< "Enter the second number : ";
        cin >> b;
    }

    // Overload the prefix decrement operator
    void operator ++ () //returnType operator symbol (arguments)
    {
        a= ++a;
        b= ++b;
    }

    void output() 
    {
      cout<<"The decremented elements of the object are:  "<< endl << a <<" and " << b << endl;
    }
};

int main() {
    OverLoad obj;
    obj.input();
    ++obj; // to Call the "void operator ++ ()" function
    obj.output();

    return 0;
}


#include<iostream>
using namespace std;

class increase {
	private:
		int value;
	public:
	increase()
	{
		value=10;
	}
	
	void operator ++()
	{
		value=value+5;
	}
	void display()
	{
		cout << "Value is : "<< value << endl;
	}
};

int main()
{
	increase inc;
	++inc; // to Call the "void operator ++ ()" function
	inc.display();
	return 0;
}
*/

/*Binary operator overloading*/
/*
#include<iostream>
using namespace std;

class complex{
	private:
		int real;
		int imag;
		
	public:
	void input()
	{
		cout << "Enter real and imaginary parts respectively : " << endl ;
		cin >> real;
		cin >> imag;
	}
	complex operator +(complex obj)
	{
		complex temp;
		temp.real=real+obj.real;
		temp.imag=imag+obj.imag;
		return temp;
	}
	void output()
	{
		cout << "output is : " << real << "+" << imag << "i";
	}
};


int main()
{
	complex com1, com2, res;
	cout << "Enter first complex number com1 : " << endl;
	com1.input();
	cout << "Enter second complex number com2 : " << endl;
	com2.input();
	
	res=com1+com2;
	res.output();
	return 0;
}
*/




/*Binary operator overloading*/
#include<iostream>
using namespace std;
class Boo
{
	int n;
	public:
	Boo()
	{
		n=0;
	}
	Boo(int a)
	{
		n=a;
	}
	void display()
	{
		cout << "No = " << n << "\n";
	}
	Boo operator +(Boo &t)
	{
		Boo temp;
		temp.n=n+t.n;
		return temp;
	}
};

int main()
{
	Boo obj1(10), obj2(20);
	Boo obj3;
	obj3=obj1+obj2;
	obj3.display();
	return 0;
}
	
