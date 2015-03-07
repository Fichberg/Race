#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#define MINIMUM_CYCLISTS 3
#define MINIMUM_METERS   249
#define EXPECTED_ARGS    4

int input_checker(int, char **);
int get_mode(char **);
int *initial_configuration(int);
int set_cyclists(int *, int, int, int);

int main(int argc, char **argv)
{
   int max_cyclists, *initial_config;
   char mode;

   /*Get initial information to feed the program*/
   max_cyclists = input_checker(argc, argv);
   mode = get_mode(argv);

   /*Starting order of the cyclists is in the array initial_config[0...max_cyclists-1]. Each cyclist is recognized by its number*/
   initial_config = initial_configuration(max_cyclists);

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

/*Returns an array with the competitors configured in an aleatory order*/
int *initial_configuration(int max_cyclists)
{
   int *initial_config;

   initial_config = malloc( max_cyclists * sizeof(int) );
   srand(time(NULL));
   set_cyclists(initial_config, 0, 0, max_cyclists);

   return initial_config;
}

/*Organize the cyclists by their number in a random order*/
int set_cyclists(int *initial_config, int pos, int p, int r)
{
   int q;

   if(p == 0) {
      if(r != 0) q = (rand() % r);
      else q = r;  
   }
   else {
      if(p != r) q = p + (rand() % (r - p));
      else q = r;
   } 

   if(q <= r && p <= q)
   {
      initial_config[pos++] = q + 1;
      pos = set_cyclists(initial_config, pos, p, q - 1);
      pos = set_cyclists(initial_config, pos, q + 1, r);

   }
   return pos;
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
   int max_cyclists;

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

   if(atoi(argv[1]) % 2 == 0) max_cyclists = atoi(argv[1]) / 2;
   else max_cyclists = (atoi(argv[1]) / 2) + 1;

   if(atoi(argv[2]) < max_cyclists) return atoi(argv[2]);
   return max_cyclists;
}