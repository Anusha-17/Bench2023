#include <iostream>
#include <vector>
#include "tinyxml2.cpp"

using namespace std;
using namespace tinyxml2;

struct mydata
{
	int Input_ID;
	int Buffer;
	int Display_ID;
};

int main()
{
  struct mydata st_data[3];
  
  XMLDocument doc;
  doc.LoadFile("example.xml");

  vector<const char*> elems = {"Input_ID", "Buffer", "Display_ID"};

  XMLElement * p_root_element = doc.RootElement();
  XMLElement * p_data = p_root_element->FirstChildElement("data"); 

  while(p_data)
  {
    for (size_t i{}; i < elems.size(); ++i)
    {
      XMLElement * ptr = p_data->FirstChildElement(elems[i]); 
      //cout << elems[i] << ": " << ptr->GetText() << '\n';
      
     cout <<  st_data[i].Input_ID <<  ptr->GetText() << '\n';
      
      cout << ( i == elems.size() - 1 ? "\n" : "");
    }
    p_data = p_data->NextSiblingElement("data");
  }

  return EXIT_SUCCESS;
}
