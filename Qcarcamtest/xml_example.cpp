/*
#include <iostream>
#include <fstream>
#include <vector>
#include "tinyxml2.cpp"

using namespace std;
using namespace tinyxml2;
   

int main(void)
{
  cout << "\nParsing my students data (sample.xml)....." << endl;
   
  // Read the sample.xml file
  XMLDocument doc;
  doc.LoadFile( "example.xml" );
   
  const char* title = doc.FirstChildElement( "MyStudentsData" )->FirstChildElement( "Student" )->GetText();
  printf( "Student Name: %s\n", title );
 
  XMLText* textNode = doc.LastChildElement( "MyStudentsData" )->LastChildElement( "Student" )->FirstChild()->ToText();
  title = textNode->Value();
  printf( "Student Name: %s\n", title );
   
  return 0;
}*/	




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
  
  int data_index=0; //index to access st_data array

  while(p_data)
  {
    struct mydata temp_data;
    for (size_t i=0; i < elems.size(); ++i)
    {
      XMLElement * ptr = p_data->FirstChildElement(elems[i]); 
      const char* text=ptr->GetText();
      cout << elems[i] << ": " << text << '\n';
      
      if(i==0)
      	temp_data.Input_ID = atoi(text);
      else if(i==1)
      	temp_data.Buffer = atoi(text);
      else if(i==2)
      	temp_data.Display_ID = atoi(text);
    }
    st_data[data_index]=temp_data;
    data_index++;
    cout << "\n";
    p_data = p_data->NextSiblingElement("data");
  }

  return EXIT_SUCCESS;
}
































static int check_error_thread(void *arg)
{
    pthread_detach(pthread_self()); 
    volatile int fatal_error_prev[NUM_MAX_CAMERAS] = {0};    
    unsigned int error_check_delay_us = CSI_ERR_CHECK_DELAY; 

    while (!g_aborted)     {
        
        for (int i = 0; i < gCtxt.numInputs; i++)
        {
            test_fatal_error_check(&gCtxt.inputs[i], &fatal_error_prev[i]); 
        }
        usleep(error_check_delay_us); 
    }
    return 0;
}
