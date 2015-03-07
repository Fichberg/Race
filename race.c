#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define MINIMUM_CYCLISTS 3
#define MINIMUM_METERS   249
#define EXPECTED_ARGS    4

int input_checker(int, char **);
int get_mode(char **);

int main(int argc, char **argv)
{
   int max_cyclists;
   char mode;

   max_cyclists = input_checker(argc, argv);
   mode = get_mode(argv);

   /*Now the program must run the selected mode*/
   if(mode == 'u' || mode == 'U')
   {
      /*do_uniform*/
   }
   else
   {
      /*do_not_uniform*/
   }

   return 0;
}

/*Gets the selected mode for the simulation*/
int get_mode(char **argv)
{
   if(strcasecmp(argv[3], "u") != 0 && strcasecmp(argv[3], "v") != 0) {
      printf("Mode argument is expected to be 'u' or 'v'.\n");
      exit(-1);
   }
   return *argv[3];
}

/*Checks input and returns the starting number of cyclists*/
int input_checker(int argc, char **argv)
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

   if(atoi(argv[1]) % 2 == 0) return atoi(argv[1]) / 2;
   return (atoi(argv[1]) / 2) + 1;
}