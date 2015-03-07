#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define MINIMUM_CYCLISTS 3
#define MINIMUM_METERS   249
#define EXPECTED_ARGS    4

char input_checker(int, char **);

int main(int argc, char **argv)
{
   char mode;

   mode = input_checker(argc, argv);

   /*Now the program must run the selected mode*/

   return 0;
}

/*Checks input and returns the selected mode*/
char input_checker(int argc, char **argv)
{
   if(argc != EXPECTED_ARGS) {
      printf("The format entrance entrance is d n [v|u].\n");
      exit(-1);
   }

   if(atoi(argv[1]) <= MINIMUM_METERS) {
      printf("The track is expected to have more than 249m (found \"%s\").\n", argv[1]);
      exit(-1);
   }

   if(atoi(argv[2]) <= MINIMUM_CYCLISTS) {
      printf("There must be at least 3 competitors (found \"%s\").\n", argv[2]);
      exit(-1);
   }

   if(strcasecmp(argv[3], "u") != 0 && strcasecmp(argv[3], "v") != 0) {
      printf("Mode argument is expected to be 'u' or 'v'.\n");
      exit(-1);
   }
      
   return *argv[3];
}