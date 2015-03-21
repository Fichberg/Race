#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#define MINIMUM_CYCLISTS 3
#define MINIMUM_METERS   249
#define EXPECTED_ARGS    4

/*struct containing the attributes of a cyclist*/
typedef struct cyclist { 
   int number;
   int position; 
   int speed; 
   int lap;
} Cyclist;

/*Global variable. Contains the number of cyclists that are still competing*/
int cyclists_competing;
/*TODO: Global variable representing the track*/


/*Functions declarations*/
int input_checker(int, char **);
char get_mode(char **);
int *initial_configuration(int);
int set_cyclists(int *, int, int, int);
void make_cyclists(Cyclist*, int*, int, int);
void create_threads(int, char, pthread_t*, Cyclist*);
void join_threads(int, pthread_t*);
void *omnium_u(void*);
void *omnium_v(void*);

/*Test functions. TODO: delete these when done*/
void print_cyclist(Cyclist);

int main(int argc, char **argv)
{
   char mode;
   int cyclists, *initial_config;
   /*threads array. Each cyclist is a thread*/
   pthread_t *my_threads;
   /*Thread arguments is the cyclist struct*/
   Cyclist *thread_args;

   /*Get initial information to feed the program*/
   cyclists_competing = cyclists = input_checker(argc, argv);
   mode = get_mode(argv);

   /*Starting order of the cyclists is in the array initial_config[0...cyclists-1]. Each cyclist is recognized by its unique number*/
   initial_config = initial_configuration(cyclists);

   /*Threads (cyclists).*/
   my_threads = malloc(cyclists * sizeof(*my_threads));
   /*Thread args (cyclist structs)*/
   thread_args = malloc(cyclists * sizeof(Cyclist));

   /*Now the program must run the selected mode*/
   if(mode == 'u')
   {
      make_cyclists(thread_args, initial_config, 50, cyclists);
      create_threads(cyclists, mode, my_threads, thread_args);
   }
   else
   {
      make_cyclists(thread_args, initial_config, 25, cyclists);
      create_threads(cyclists, mode, my_threads, thread_args);
   }

   join_threads(cyclists, my_threads);
   free(initial_config);
   free(my_threads);
   free(thread_args);
   return 0;
}

/*Omnium race function in 'u' mode*/
void *omnium_u(void *args)
{
   Cyclist cyclist = *((Cyclist*) args);

   print_cyclist(cyclist);

   return NULL;
}

/*Omnium race function in 'v' mode*/
void *omnium_v(void *args)
{
   Cyclist cyclist = *((Cyclist*) args);

   print_cyclist(cyclist);

   return NULL;
}

/*Function to create all threads*/
void create_threads(int cyclists, char mode, pthread_t *my_threads, Cyclist *thread_args)
{
   int i;
   if(mode == 'u')
   {
      for(i = 0; i < cyclists; i++)
      {
         if (pthread_create(&my_threads[i], NULL, omnium_u, &thread_args[i])) 
         {
            printf("Error creating thread.");
            abort();
         }
      }  
   }
   else
   {
      for(i = 0; i < cyclists; i++)
      {
         if (pthread_create(&my_threads[i], NULL, omnium_v, &thread_args[i])) 
         {
            printf("Error creating thread.");
            abort();
         }
      }
   }
}

/*Function to join all threads*/
void join_threads(int cyclists, pthread_t *my_threads)
{
   int i;
   for(i = 0; i < cyclists; i++)
   {
     if (pthread_join(my_threads[i], NULL)) 
     {
        printf("Error joining thread.");
        abort();
     }
   }
}

/*Add all the attributes to the cyclists.*/
void make_cyclists(Cyclist *thread_args, int *initial_config, int initial_speed, int cyclists)
{
   int i;
   for(i = 0; i < cyclists; i++)
   {
      thread_args[i].number = initial_config[i];
      thread_args[i].position = i;
      thread_args[i].speed = initial_speed;
      thread_args[i].lap = 1; /*first lap*/
   }
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
char get_mode(char **argv)
{
   if(strcasecmp(argv[3], "u") != 0 && strcasecmp(argv[3], "v") != 0) {
      printf("Mode argument is expected to be 'u' or 'v'.\n");
      exit(-1);
   }
   if(strcasecmp(argv[3], "u") == 0) return 'u';
   return 'v';
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


/*All the functions below this line are here for debugging purposes. TODO: delete these when done.*/
void print_cyclist(Cyclist cyclist)
{
   printf("Cyclist number = %d\n", cyclist.number);
   printf("Cyclist position = %d\n", cyclist.position);
   printf("Cyclist speed = %d\n", cyclist.speed);
   printf("Cyclist lap = %d\n\n", cyclist.lap);
}