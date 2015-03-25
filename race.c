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
#define NO_CYCLISTS      0
#define LOCKED           1
#define UNLOCKED         0

/*struct containing the attributes of a cyclist*/
typedef struct cyclist { 
   int number;    /*Cyclist number*/
   int position;  /*Position in the track. [0...track_size-1]. Position is printed as*/
   int place;     /*His place of the race (1 for first, 2 for second... cyclist_competing for last (in actual lap))*/
   int speed;     /*Cyclist speed. 25km/h or 50km/h*/
   int lap;       /*His actual lap*/
   char broken;   /*did he broke?*/
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
int elapsed_time;
/*Global variable. Represents the track*/
Track track;
/*Global variable. Contains the size of the track. It is an array of Meter with size [0...track_size-1]*/
int track_size;
/*Global variable. Says if the race has started*/
int go;
/*Global variable. Protexts the writing in track array*/
pthread_mutex_t lock;
/*Global variable. All cyclists moved*/
int moved_cyclists;

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
void move_cyclist(Cyclist*);
void semaphore(Cyclist*);
void await(int);

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

   /*Race chronometer*/
   elapsed_time = 0;

   /*Cyclists made no movement yet*/
   moved_cyclists = 0;

   /*Lock is initialized*/
   if (pthread_mutex_init(&lock, NULL) != 0)
   {
      printf("\n mutex init failed\n");
      return 1;
   }

   /*Sets the size of the track*/
   track_size = atoi(argv[1]);

   /*Allocates the track*/
   make_track();
   /*Now the program must run the selected mode*/
   if(mode == 'u')
   {
      printf("\nPlacing competitors...\n\n");
      sleep(1);
      make_cyclists(thread_args, initial_config, 50, cyclists);
      put_cyclists_in_track(thread_args, cyclists);
      create_threads(cyclists, mode, my_threads, thread_args);
      sleep(1);
      printf("\nAdjusting chronometer... ");
      sleep(3);
      if (pthread_create(&time_thread, NULL, omnium_chronometer, NULL)) 
      {
         printf("Error creating time thread.");
         abort();
      }
   }
   else
   {
      printf("\nPlacing competitors...\n\n");
      sleep(1);
      make_cyclists(thread_args, initial_config, 25, cyclists);
      put_cyclists_in_track(thread_args, cyclists);
      create_threads(cyclists, mode, my_threads, thread_args);
      sleep(1);
      printf("\nAdjusting chronometer...\n\n");
      sleep(3);
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
   pthread_mutex_destroy(&lock);
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

/*Attempts to move a cyclist*/
void move_cyclist(Cyclist *cyclist)
{
   int next_position = (cyclist->position + 1) % track_size;
   if(track[next_position].cyclists < 4) 
   {
      /*This cyclist forwarded 1 position*/
      (track[cyclist->position].cyclists)--;
      (track[next_position].cyclists)++;
      cyclist->position = next_position;


      if((track[cyclist->position].cyclists) > MAX_CYCLISTS) 
      {
         printf("\nError. Found more than 4 cyclists in track[%d].\n", cyclist->position);
         exit(0);
      }
      if(cyclist->position - 1 > 0 && ((track[cyclist->position - 1].cyclists) < NO_CYCLISTS)) 
      {
         printf("\nError. Found negative number of cyclists in track[%d].\n", cyclist->position - 1);
         exit(0);
      }
      else if(cyclist->position - 1 == 0 && ((track[track_size-1].cyclists) < NO_CYCLISTS)) 
      {
         printf("\nError. Found negative number of cyclists in track[%d].\n", track_size - 1);
         exit(0);
      }

      if(track[cyclist->position - 1].cyclist1 == cyclist) track[cyclist->position - 1].cyclist1 = NULL;
      else if(track[cyclist->position - 1].cyclist2 == cyclist) track[cyclist->position - 1].cyclist2 = NULL;
      else if(track[cyclist->position - 1].cyclist3 == cyclist) track[cyclist->position - 1].cyclist3 = NULL;
      else track[cyclist->position - 1].cyclist4 = NULL; 
      
      if(track[cyclist->position].cyclist1 == NULL) track[cyclist->position].cyclist1 = cyclist;
      else if(track[cyclist->position].cyclist2 == NULL) track[cyclist->position].cyclist2 = cyclist;
      else if(track[cyclist->position].cyclist3 == NULL) track[cyclist->position].cyclist3 = cyclist;
      else track[cyclist->position].cyclist4 = cyclist;

      moved_cyclists++;
      print_cyclist(*cyclist);
   }
}

/*Semaphore*/
void semaphore(Cyclist *cyclist)
{
   pthread_mutex_lock(&lock);
   move_cyclist(cyclist);
   pthread_mutex_unlock(&lock);
}

/*Omnium race function in 'u' mode*/
void *omnium_u(void *args)
{
   Cyclist *cyclist = ((Cyclist*) args);

   print_cyclist(*cyclist);
   while(!go) continue;

   while(cyclists_competing != 1) 
   {
      /*Cyclist can attempt to move to next cell in the track array*/
      semaphore(cyclist);
      while(moved_cyclists != 0) continue;
      await(cyclists_competing * 300000);
   }

   return NULL;
}

/*Omnium race function in 'v' mode*/
void *omnium_v(void *args)
{
   int n = 1;
   Cyclist *cyclist = ((Cyclist*) args);

   while(!go) continue;
   print_cyclist(*cyclist);

   while(cyclists_competing != 1) continue;

   return NULL;
}

/*Function to wait x ms.*/
void await(int x)
{
   struct timespec tim, tim2;
   tim.tv_sec = tim2.tv_sec = 0;
   tim.tv_nsec = tim2.tv_nsec = x; /*72ms*/

   if (nanosleep(&tim , &tim2) < 0)
   {
      printf("Nanosleep failed.\n");
      exit(-1);
   }
}

/*Runs the chronometer. The chronometer also prints on the screen the information about the race.*/
void *omnium_chronometer(void *args)
{
   int milliseconds_adder = 72;

   /*Race will start. After countdown(), all cyclist threads will be unlocked.*/
   countdown();
   
   /*Time thread will run until we have just 1 cyclist competing*/
   while(cyclists_competing != 1)
   {
      /*Simulation cycle*/
      await(72000000);
      elapsed_time += milliseconds_adder;
      printf("Elapsed time: %.3f\n-----------------------\n",  elapsed_time / 100.0);
      while(moved_cyclists != cyclists_competing) continue;
      moved_cyclists = 0;
   }


   return NULL;
}

/*Countdown for race start. While the countdown is running, cyclists threads are being created.*/
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
   printf("GO!\n\n");
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
      thread_args[i].place = cyclists - i;
      thread_args[i].speed = initial_speed;
      thread_args[i].lap = 1; /*first lap*/
      thread_args[i].broken = 'N';
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
   printf("Cyclist #%d | Track Position:  %d | Speed: %d | Lap: %d | Broken? %c\n", cyclist.number, cyclist.position, cyclist.speed, cyclist.lap, cyclist.broken);
}

/*Prints the information about the cyclists that are still competing*/
void print_track()
{
   int i, c = 0;
   printf("Track configuration (just positions containing cyclists):\n\n");
   for(i = track_size-1; i > -1; i--)
   {
      if(c == cyclists_competing) break;
      if(track[i].cyclists == 0) continue;
      printf("Track position (index): %d\n", i);
      printf("Cyclists in this position: %d\n", track[i].cyclists);
      if(track[i].cyclists >= 1) { 
         printf("Cyclist number %d is in %d place. Broken? %c  (%p)\n", (*track[i].cyclist1).number, (*track[i].cyclist1).place, (*track[i].cyclist1).broken, (void*)track[i].cyclist1); 
         ++c; 
      } 
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 2) { 
         printf("Cyclist number %d is in %d place. Broken? %c (%p)\n", (*track[i].cyclist2).number, (*track[i].cyclist2).place, (*track[i].cyclist2).broken, (void*)track[i].cyclist2); 
         ++c; 
      }
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 3) { 
         printf("Cyclist number %d is in %d place. Broken? %c (%p)\n", (*track[i].cyclist3).number, (*track[i].cyclist3).place, (*track[i].cyclist3).broken, (void*)track[i].cyclist3); 
         ++c; 
      }
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 4) { 
         printf("Cyclist number %d is in %d place. Broken? %c (%p)\n\n", (*track[i].cyclist4).number, (*track[i].cyclist4).place, (*track[i].cyclist4).broken, (void*)track[i].cyclist4); 
         ++c; 
      }
   }
}

/*
TODO:
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
7)
UM CICLISTA É ELIMINADO QUANDO ELE TEM POSITION == track_size-1 e 
------------------------------------------------------
8)
MODIFICAR LAP QUANDO COMPLETA UMA VOLTA


QUESTOES:
- As informações de print_track() precisam mesmo ser printadas no terminal ou em um arquivo de saída?
- Implementei o semaforo nã mão. Pode fazer isso ou precisa usar alguma biblioteca existente pra isso?



Informações relatório:
O programa foi testado com n <= 380. Acima disso, provavelmente você pode receber core dumped

Um ciclista que inicialmente estava ATRAS, ao ficar na mesma posição de um que estava a frente dele, vai manter a sua posição no placar.
Sua posição só será alterada se ele fizer uma ULTRAPASSAGEM COMPLETA.



PROBLEMA:
Atual implelentação corre risco de elapsed time dar arithmetic overflow







tempo:


http://stackoverflow.com/questions/1486833/pthread-cond-timedwait-help





*/