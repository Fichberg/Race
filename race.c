#define _XOPEN_SOURCE 500 /*To compile without nanosleep implicit declaration warning*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define MINIMUM_CYCLISTS 3
#define MINIMUM_METERS   249
#define EXPECTED_ARGS    4
#define START            1
#define MAX_CYCLISTS     4
#define NOT_BROKEN       0
#define BROKEN           1

/*struct containing the attributes of a cyclist*/
typedef struct cyclist { 
   int number;    /*Cyclist number*/
   int position;  /*Position in the track. [0...track_size-1]*/
   int speed;     /*Cyclist speed. 25km/h or 50km/h*/
   int lap;       /*His actual lap*/
} Cyclist;

/*Each position of the track is a cell of type meter*/
typedef struct meter { 
   Cyclist* cyclist1; 
   Cyclist* cyclist2; 
   Cyclist* cyclist3; 
   Cyclist* cyclist4; 
   int cyclists;  /*Number of cyclists in this position of the track*/
} Meter;

/*Definition of the track*/
typedef Meter* Track;

/*Global variable. Contains the number of cyclists that are still competing*/
int cyclists_competing;
/*Global variable. Contains the race time duration*/
double elapsed_time;
/*Global variable. Represents the track*/
Track track;
/*Global variable. Contains the size of the track. It is an array of Meter with size [0...track_size-1]*/
int track_size;
/*Global variable. Says if the race has started*/
int go;

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
void create_time_thread(pthread_t);
void join_time_thread(pthread_t);
void *omnium_chronometer(void*);
void countdown();
void make_track();
void print_track();
void put_cyclists_in_track(Cyclist*, int);

/*Test functions. TODO: delete these when done*/
void print_cyclist(Cyclist);

int main(int argc, char **argv)
{
   char mode;
   int cyclists, *initial_config;
   /*threads array. Each cyclist is a thread.*/
   pthread_t *my_threads;
   /*thread in charge of the time elapsed in the simulation*/
   pthread_t time_thread;
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

   /*Sets go to false. Cyclists can't start unless go is true*/
   go = 0;

   /*Sets the size of the track*/
   track_size = atoi(argv[1]);

   /*Allocates the track*/
   make_track();
   /*Now the program must run the selected mode*/
   if(mode == 'u')
   {
      make_cyclists(thread_args, initial_config, 50, cyclists);
      put_cyclists_in_track(thread_args, cyclists);
      create_threads(cyclists, mode, my_threads, thread_args);
      if (pthread_create(&time_thread, NULL, omnium_chronometer, NULL)) 
      {
         printf("Error creating time thread.");
         abort();
      }
   }
   else
   {
      make_cyclists(thread_args, initial_config, 25, cyclists);
      put_cyclists_in_track(thread_args, cyclists);
      create_threads(cyclists, mode, my_threads, thread_args);
      if (pthread_create(&time_thread, NULL, omnium_chronometer, NULL)) 
      {
         printf("Error creating time thread.");
         abort();
      }  
   }

   join_threads(cyclists, my_threads);
   free(initial_config);
   free(my_threads);
   free(thread_args);
   free(track);
   return 0;
}

/*Allocates the track*/
void make_track()
{
   int i;
   track = malloc(track_size * sizeof(Meter));
   for(i = 0; i < track_size; i++)
   {
      track[i].cyclist1 = NULL;
      track[i].cyclist2 = NULL;
      track[i].cyclist3 = NULL;
      track[i].cyclist4 = NULL;
      track[i].cyclists = 0;
   }
}

/*Omnium race function in 'u' mode*/
void *omnium_u(void *args)
{
   Cyclist cyclist = *((Cyclist*) args);

   while(!go) continue;

   /*print_cyclist(cyclist);*/

   while(cyclists_competing != 1) 
   {
      continue;
   }

   return NULL;
}

/*Omnium race function in 'v' mode*/
void *omnium_v(void *args)
{
   Cyclist cyclist = *((Cyclist*) args);

   while(!go)
   {
      continue;
   }

   /*print_cyclist(cyclist);*/

   while(cyclists_competing != 1) continue;

   return NULL;
}

/*Runs the chronometer*/
void *omnium_chronometer(void *args)
{
   double milliseconds_adder = 0.072;
   struct timespec tim, tim2;
   tim.tv_sec = tim2.tv_sec = 0;
   tim.tv_nsec = tim2.tv_nsec = 72000000; /*72ms*/

   /*Race will start. After countdown(), all cyclist threads will be unlocked.*/
   countdown();
   
   /*Race chronometer*/
   elapsed_time = 0;
   
   /*Time thread will run until we have just 1 cyclist competing*/
   while(cyclists_competing != 1)
   {
      /*Simulation cycle*/
      if (nanosleep(&tim , &tim2) < 0)
      {
         printf("Nanosleep failed.\n");
         exit(-1);
      }
      elapsed_time += milliseconds_adder;
      printf("Elapsed time: %.3f\n",  elapsed_time);
      print_track();
   }


   return NULL;
}

void countdown()
{
   int i;
   printf("\nOmnium will start in 5 seconds!\n\n");
   for(i = 5; i >= 2; i--)
   {
      sleep(1);
      printf("%d...\n", i);
   }
   sleep(1);
   printf("\nGO!\n");
   go = START;
}

void put_cyclists_in_track(Cyclist *thread_args, int cyclists)
{
   int i;
   for(i = 0; i < cyclists; i++)
   {
      track[i].cyclist1 = &thread_args[i];
      track[i].cyclists = 1;
   }
}

/*Function to join the time thread*/
void join_time_thread(pthread_t time_thread)
{
  if (pthread_join(time_thread, NULL)) 
  {
      printf("Error joining time thread.");
      abort();
  }
}

/*Function to create all Cyclists threads*/
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
      printf("The track is expected to have more than 249m (found \"%sm\").\n", argv[1]);
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

/*Prints the information about the cyclists that are still competing*/
void print_track()
{
   int i, c = 0;
   printf("Track configuration (just positions containing cyclists):\n\n");
   for(i = 0; i < track_size; i++)
   {
      if(c == cyclists_competing) break;
      if(track[i].cyclists == 0) continue;
      printf("Track position (array index + 1): %d\n", i + 1);
      printf("Cyclists in this position: = %d\n", track[i].cyclists);
      if(track[i].cyclists >= 1) { printf("Cyclist number = %d   (%p)\n", (*track[i].cyclist1).number, (void*)track[i].cyclist1); ++c; } 
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 2) { printf("Cyclist number = %d   (%p)\n", (*track[i].cyclist2).number, (void*)track[i].cyclist2); ++c; }
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 3) { printf("Cyclist number = %d   (%p)\n", (*track[i].cyclist3).number, (void*)track[i].cyclist3); ++c; }
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 4) { printf("Cyclist number = %d   (%p)\n\n", (*track[i].cyclist4).number, (void*)track[i].cyclist4); ++c; }
   }
}

/*
TODO:
2)
Thread para controlar o tempo, junto com uma variável global time.
Sempre que time for múltiplo de 72ms, as informações da pista são impressas (não sei se na tela ou em arquivo. A ver)
-----------------------------------------------------
3)
Sempre que o lap de um ciclista for

lap % 4 == 1

Ele tem 1% de chance de quebrar (isso pq o lap começa no 1)

OBS: QUANDO SOBRAREM TRES CICLISTAS NA PISTA NÃO EXISTE MAIS A CHANCE DE QUEBRAR!!!!!!!!!!!!!!
-----------------------------------------------------
4)
Ciclistas quebrados ou eliminados tem a sua thread destruída!
-----------------------------------------------------
5) (Provavelmente está será a última coisa que faremos)
MODO DEBUG:
Seu programa deve ainda permitir uma opcao de debug na qual a cada 14,4 segundos deve ser exibido na tela a volta em que
cada ciclista está e a posicao dele naquela volta atual.
------------------------------------------------------
6)
LER SAÍDA NO ENUNCIADO
------------------------------------------------------






Informações relatório:
O programa foi testado com n <= 380. Acima disso, provavelmente você pode receber core dumped









tempo:


http://stackoverflow.com/questions/1486833/pthread-cond-timedwait-help





*/