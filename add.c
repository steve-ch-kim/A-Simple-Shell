#include <stdio.h>
#include <string.h>
#include <stdlib.h>
 
int main(int argc, char * argv[]){ 
   
   int n = atoi(argv[1]);
   printf("%d \n", n + 2);
   return 0;
   
   // char buffer[10];
   // fgets(buffer, 10, stdin);
   // int n;
   // n = atoi(buffer); 
   // printf("%d \n", n + 2);  // print n+2 
   // return 0; 
}