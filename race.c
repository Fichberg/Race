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
   int number;       /*Cyclist number*/
   float position;     /*Position in the track. [0...track_size-1]. Position is printed as*/
   int place;        /*His place of the race (1 for first, 2 for second... cyclist_competing for last (in actual lap))*/
   int speed;        /*Cyclist speed. 25km/h or 50km/h*/
   int lap;          /*His actual lap*/
   char eliminated;  /*is he eliminated?*/
   char broken;      /*did he broke?*/
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
/*Global variable. Protects the writing in track array*/
pthread_mutex_t lock;
/*Global variable. A counter to the number of cyclists that moved in a time cycle*/
int moved_cyclists;
/*Global variable. Contains the number of the last cyclist to move*/
int last;

/*Functions declarations*/
int input_checker(int, char **);
char get_mode(char **);
int *initial_configuration(int);
int set_cyclists(int *, int, int, int, int);
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
void put_cyclists_in_track(Cyclist*, int);
void move_cyclist(Cyclist*);
void critical_section(Cyclist*);
void await(int);
void print_cyclist(Cyclist);
float convert_index_to_meters(int);
int convert_meters_to_index(float);
int lap_complete(int);
void increment_lap(Cyclist*);
void move(Cyclist*, int, int);
float decide_next_position25(Cyclist*, int);
float decide_next_position50(Cyclist*, int);
int disqualified(Cyclist*);
void eliminate_cyclist(Cyclist*, int, int);
void eliminate(Cyclist*);
void broadcast(Cyclist*);

/*Test functions. TODO: delete these when done*/
void print_track();

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

   /*Lock is initialized*/
   if (pthread_mutex_init(&lock, NULL) != 0)
   {
      printf("\n mutex init failed\n");
      return 1;
   }

   /*Sets the size of the track*/
   /*Multiplied by 2 because cyclist can move 0.5m when they are with a speed of 25km/h*/
   track_size = (atoi(argv[1]) * 2);

   /*Initialize global variable*/
   moved_cyclists = 0;

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
/*Note: A cyclist with speed = 25 walks 0.5cells (0.5m in a cycle of 72ms) in track array, 
while speed 50km walks a full cell (1m in a cycle of 72ms), according to the enuntiacion*/
void move_cyclist(Cyclist *cyclist)
{
   float position1 = cyclist->position, position2;
   if(cyclist->speed == 25)
   {
      position2 = decide_next_position25(cyclist, convert_meters_to_index(cyclist->position));
      /*The cyclists can only advance if there's at least 1 empty slot ahead*/
      if(track[convert_meters_to_index(position2)].cyclists < 4) 
      {
         /*If he is going to complete a lap, increments*/
         if(lap_complete(convert_meters_to_index(position2))) increment_lap(cyclist);

         /*This cyclist will be forwarded 0.5m*/
         move(cyclist, convert_meters_to_index(position1), convert_meters_to_index(position2));

         /*swap places of cyclists in case of a total overtaking*/
         /*TODO*/

         /*See if this cyclist will break (1% chance).*/
         /*TODO*/

         /*See if this cyclist is eliminated*/
         eliminate_cyclist(cyclist, convert_meters_to_index(position1), convert_meters_to_index(position2));

         /*print_cyclist(*cyclist);*/
      }
   }
   else /*cyclist->speed == 50*/
   {
      position2 = decide_next_position50(cyclist, convert_meters_to_index(cyclist->position));
      if(track[convert_meters_to_index(position2 - 0.5)].cyclists < 4) 
      {
         if(track[convert_meters_to_index(position2)].cyclists < 4) 
         {
            /*If he is going to complete a lap, increments*/
            if(lap_complete(convert_meters_to_index(position2))) increment_lap(cyclist);

            /*This cyclist is will be forwarded 0.5m*/
            move(cyclist, convert_meters_to_index(position1), convert_meters_to_index(position2));

            /*swap places of cyclists in case of a total overtaking*/
            /*TODO*/

            /*See if this cyclist will break (1% chance).*/
            /*TODO*/

            /*See if this cyclist is eliminated*/
            eliminate_cyclist(cyclist, convert_meters_to_index(position1), convert_meters_to_index(position2));

            print_cyclist(*cyclist);
         }
         else /*He will be able to advance only half a meter*/
         {
            /*If he is going to complete a lap, increments*/
            if(lap_complete(convert_meters_to_index(position2))) increment_lap(cyclist);
            
            /*This cyclist is will be forwarded 1m*/
            move(cyclist, convert_meters_to_index(position1), convert_meters_to_index(position2));

            /*swap places of cyclists in case of a total overtaking*/
            /*TODO*/

            /*See if this cyclist will break (1% chance).*/
            /*TODO*/

            /*See if this cyclist is eliminated*/
            eliminate_cyclist(cyclist, convert_meters_to_index(position1), convert_meters_to_index(position2));

            print_cyclist(*cyclist);
         }
      }
   }
}

/*Broadcasts, announcing broken, and eliminated cyclists and the winner of the race*/
void broadcast(Cyclist *cyclist)
{
   if(cyclist->eliminated == 'Y')
      printf("\n***************************\nThe cyclist %d (%p) has been ELIMINATED.\n***************************\n", cyclist->number, (void*)cyclist);
   else if(cyclist->broken == 'Y')
      printf("\n***************************\nThe cyclist %d (%p) has  BROKEN.\n***************************\n", cyclist->number, (void*)cyclist);
   else
      printf("\n***************************\nThe cyclist %d (%p) has WON THE RACE.\n***************************\n", cyclist->number, (void*)cyclist);
}

/*Confirms if the cyclists is out*/
int disqualified(Cyclist *cyclist)
{
   if(cyclist->eliminated == 'Y' || cyclist->broken == 'Y') return 1;
   return 0;
}

/*Eliminates the worst cyclist of the lap*/
void eliminate_cyclist(Cyclist *cyclist, int position1, int position2)
{
   /*Confirms positions. Did he really crossed the line?*/
   if(((position1 == track_size-1) && ((position2 == 0) || (position2 == 1))) || ( position1 == track_size-2 && position2 == 0 ))
      if((cyclist->lap > 1) && (cyclist->place == cyclists_competing) && (cyclist->broken == 'N'))
      {
         /*do eliminate*/
         eliminate(cyclist);
      }
}

/*Marks and eliminated the cyclist from the competition*/
void eliminate(Cyclist *cyclist)
{
   int index = convert_meters_to_index(cyclist->position);
   /*Look for the cyclist in the track to remove him*/
   if(track[index].cyclist1 == cyclist) track[index].cyclist1 = NULL;
   else if(track[index].cyclist2 == cyclist) track[index].cyclist2 = NULL;
   else if(track[index].cyclist3 == cyclist) track[index].cyclist3 = NULL;
   else track[index].cyclist4 = NULL;
   /*Marks him to eliminate him after*/
   cyclist->eliminated = 'Y';
   moved_cyclists--; /*to keep balanced the condition in omnium_chronometer*/
   cyclists_competing--;
}

/*Decides the next position  for speed = 25 and if he is at track_size-1*/
float decide_next_position25(Cyclist *cyclist, int position1)
{
   if(position1 == track_size-1) return 0.5;
   return cyclist->position + 0.5;
}
/*Decides the next position for speed = 50 and if he is at track_size-1 or track_size-2*/
float decide_next_position50(Cyclist *cyclist, int position1)
{
   /*He can advance 1m*/
   if(position1 < track_size-2 && track[position1 + 2].cyclists < 4) return cyclist->position + 1.0;
   else if (position1 == track_size-2 && track[0].cyclists < 4) return 0.5;
   else if (position1 == track_size-1 && track[1].cyclists < 4) return 1.0;
   /*He can only advance 0.5m*/
   else
   {
      if(position1 == track_size-1) return 0.5;
      return cyclist->position + 0.5;  
   }
}

/*Moves the cyclist*/
void move(Cyclist *cyclist, int position1, int position2)
{
   (track[position1].cyclists)--;
   (track[position2].cyclists)++;
   cyclist->position = convert_index_to_meters(position2);

   if((track[position2].cyclists) > MAX_CYCLISTS) 
   {
      printf("\nError. Found more than 4 cyclists in track[%d].\n", position2);
      exit(0);
   }
   if(position1 > 0 && ((track[position1].cyclists) < NO_CYCLISTS)) 
   {
      printf("\nError. Found negative number of cyclists in track[%d].\n", position1);
      exit(0);
   }

   if(track[position1].cyclist1 == cyclist) track[position1].cyclist1 = NULL;
   else if(track[position1].cyclist2 == cyclist) track[position1].cyclist2 = NULL;
   else if(track[position1].cyclist3 == cyclist) track[position1].cyclist3 = NULL;
   else track[position1].cyclist4 = NULL; 
   
   if(track[position2].cyclist1 == NULL) track[position2].cyclist1 = cyclist;
   else if(track[position2].cyclist2 == NULL) track[position2].cyclist2 = cyclist;
   else if(track[position2].cyclist3 == NULL) track[position2].cyclist3 = cyclist;
   else track[position2].cyclist4 = cyclist;

}

/*Checks is the cyclist in this position will complete a new lap*/
int lap_complete(int position)
{
   if(track_size-1 == position) return 1;
   return 0;
}

void increment_lap(Cyclist *cyclist)
{
   (cyclist->lap)++;
}

/*CS*/
void critical_section(Cyclist *cyclist)
{
   pthread_mutex_lock(&lock);
   move_cyclist(cyclist);
   if(moved_cyclists + 1 == cyclists_competing) last = cyclist->number;
   moved_cyclists++;
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
      /*Critical section*/
      critical_section(cyclist);
      await(4000000); /*4ms*/
      while(moved_cyclists != 0) if(cyclist->number == last) last = -1;
      /*Give time to all threads leave the last while*/
      if(disqualified(cyclist) == 1) break;
      await(4000000); /*4ms*/
   }
   broadcast(cyclist);
   return NULL;
}

/*Omnium race function in 'v' mode*/
void *omnium_v(void *args)
{
   Cyclist *cyclist = ((Cyclist*) args);

   print_cyclist(*cyclist);
   while(!go) continue;

   while(cyclists_competing != 1) 
   {
      continue;
   }

   return NULL;
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
      /*All cyclists must have moved within this cycle*/
      while(moved_cyclists != cyclists_competing) continue;
      /*Wait the thread of the last cyclist "group" with the others*/
      while(last != -1) continue;
      /*Release the cyclists!*/
      moved_cyclists = 0;
   }

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
   int i, j;
   for(i = 0, j = 1; i < cyclists; i++, j+=2)
   {
      track[j].cyclist1 = &thread_args[i];
      track[j].cyclists = 1;
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
   int i, j;
   for(i = 0, j = 1; i < cyclists; i++, j+=2)
   {
      thread_args[i].number = initial_config[i];
      thread_args[i].position = convert_index_to_meters(j); /*At the start of the race, all cyclists starts with 1m space of each other independent of the mode*/
      thread_args[i].place = cyclists - i;
      thread_args[i].speed = initial_speed;
      thread_args[i].lap = 1; /*first lap*/
      thread_args[i].eliminated = 'N';
      thread_args[i].broken = 'N';
   }
}

/*Returns an array with the competitors configured in an aleatory order*/
int *initial_configuration(int max_cyclists)
{
   int *initial_config;

   initial_config = malloc( max_cyclists * sizeof(int) );
   srand(time(NULL));
   set_cyclists(initial_config, 0, 0, max_cyclists, max_cyclists);

   return initial_config;
}

/*Converts the position of the cyclist or the track index to a float position*/
float convert_index_to_meters(int position)
{
   if(position % 2 == 1) return ((int)(position / 2)) + 1.0;
   return ((int)(position / 2)) + 0.5;
}

/*Converts the the cyclist position to a track index*/
int convert_meters_to_index(float position)
{
   return (int)((position * 2) - 1);
}

/*Organize the cyclists by their number in a random order*/
int set_cyclists(int *initial_config, int pos, int p, int r, int size)
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
      if(pos < size) initial_config[pos++] = q + 1;
      pos = set_cyclists(initial_config, pos, p, q - 1, size);
      pos = set_cyclists(initial_config, pos, q + 1, r, size);

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
   printf("Cyclist #%d | Track Position:  %.1fm | Place: %d | Speed: %d | Lap: %d\n", cyclist.number, cyclist.position, cyclist.place, cyclist.speed, cyclist.lap);
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
      printf("Track position %.1f meters (index = %d):\n", convert_index_to_meters(i), i);
      printf("Cyclists in this position: %d\n", track[i].cyclists);
      if(track[i].cyclists >= 1) { 
         printf("Cyclist number %d is in %d place. (%p)\n", (*track[i].cyclist1).number, (*track[i].cyclist1).place, (void*)track[i].cyclist1); 
         ++c; 
      } 
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 2) { 
         printf("Cyclist number %d is in %d place. (%p)\n", (*track[i].cyclist2).number, (*track[i].cyclist2).place, (void*)track[i].cyclist2); 
         ++c; 
      }
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 3) { 
         printf("Cyclist number %d is in %d place. (%p)\n", (*track[i].cyclist3).number, (*track[i].cyclist3).place, (void*)track[i].cyclist3); 
         ++c; 
      }
      else { printf("\n"); continue; }
      if(track[i].cyclists >= 4) { 
         printf("Cyclist number %d is in %d place. (%p)\n\n", (*track[i].cyclist4).number, (*track[i].cyclist4).place, (void*)track[i].cyclist4); 
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
------------------------------------------------------
9)
Colocar seed time nas rands!


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