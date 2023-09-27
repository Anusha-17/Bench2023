#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct st
{
	int input_id;
	int fps;
	int display_id;
}qc;
int main()
{
 struct st qc;
 FILE* fp;
 char s[2048];
 
 // Open XML file to read
 fp = fopen("test.xml", "r");
 
 if ( fp == NULL ) printf("File error, can't read! \n");
 else
 {
 	printf("Printing file content:\n");
 
 // read each lines of XML file
	 while( !feof(fp) )
	 {

	 fgets( s, sizeof("test.xml"), fp ); //max 2047 char
	 printf("%s", s);

	 };
	  printf("qc.input_id=%d",qc.input_id);
	 
 
 }
 fclose(fp);
 getchar();
 return 0;
}

#include<iostream>
#include<boost/property_tree_/ptree.hpp>
using boost::property_tree::ptree;
#include<boost/pro
