#define _XOPEN_SOURCE 500 /*To compile without nanosleep implicit declaration warning*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>

#define MINIMUM_CYCLISTS 3
#define MINIMUM_METERS   249
#define EXPECTED_ARGS    4
#define START            1
#define STOP             0
#define MAX_CYCLISTS     4
#define NO_CYCLISTS      0

/*struct containing the attributes of a cyclist*/
typedef struct cyclist { 
   int number;                /*Cyclist number*/
   int position;              /*Position in the track. [0...track_size-1]. Position is printed as*/
   int place;                 /*His place of the race (1 for first, 2 for second... cyclist_competing for last (in actual lap))*/
   int speed;                 /*Cyclist speed. 25km/h or 50km/h*/
   int lap;                   /*His actual lap*/
   char eliminated;           /*is he eliminated?*/
   char broken;               /*did he broke?*/
   clock_t cyclist_timer;     /*Elimination, broken or victory time*/
   float half;                /*Stores extra position for speed = 25*/
} Cyclist;

/*Each position of the track is a cell of type meter*/
typedef struct meter { 
   sem_t mutex;                  /*Semaphore to allow a limited number os cyclists to get into this meter*/
   pthread_mutex_t meter_lock;   /*Special lock to guarantee safe writing in this meter*/
   Cyclist* cyclist1;            /*Field to assign 1st cyclist to this meter*/
   Cyclist* cyclist2;            /*Field to assign 2nd cyclist to this meter*/
   Cyclist* cyclist3;            /*Field to assign 3rd cyclist to this meter*/
   Cyclist* cyclist4;            /*Field to assign 4th cyclist to this meter*/
   int cyclists;                 /*Number of cyclists in this position of the track*/
} Meter;

/*Definition of the track*/
typedef Meter* Track;

/*Global variables related to number of cyclists. 
cyclists_competing stores the number of cyclists still running (i.e not broken and not eliminated). 
total_cyclists stores the total number of cyclists, passed through command line*/
int cyclists_competing, total_cyclists;
/*Global variable related to time. Contains the race time duration*/
clock_t start_timer;
/*Global variables related to the track. 
track represents the track (an array of struct meter)
track_size contains the size of the track. It goes from [0...track_size-1]*/
Track track;
int track_size;
/*Global variable that allows or not the cyclists to run. Used at the beggining of the race and of each cycle of the simulation (cycles of 0.72ms, as defined)*/
int go;
/*Global variable related to simulation mode. Stores the mode. u and v for normal run, U and V for debug run*/
char mode;
/*Global variables. 
try_to_break contains the cyclist (using his race position, aka cyclist->place) that will suffer a break attempt.
update is used to allow the simulation to update all cyclists race position, given a cyclist has broken*/
int try_to_break, update;
/*Global variable: gives permission to eliminate a cyclists. This is to avoid double elimination (when there's a tie in the last position*/
int already_eliminated;
pthread_mutex_t elimination_lock;

/*Functions prototypes*/
int roll_speed();
int roll_cyclist_to_try_to_break();
int *initial_configuration(int);
int set_cyclists(int *, int, int, int, int);
void make_track();
void make_cyclists(Cyclist*, int*, int, int);
void put_cyclists_in_track(Cyclist*, int);
void create_threads(int, pthread_t*, Cyclist*);
void join_threads(int, pthread_t*);
void create_time_thread(pthread_t);
void join_time_thread(pthread_t);
void join_log_thread(pthread_t);
void *omnium(void*);
void *omnium_chronometer(void*);
void *omnium_logger(void*);
void countdown();
void await(int);
int disqualified(Cyclist*);
void overtake(Cyclist*, int);
void break_cyclist(Cyclist*);
void broadcast(Cyclist*);
void mark_cyclist(Cyclist*, char);
void eliminate_cyclist(Cyclist*, int);
int decide_new_position(Cyclist*);
void critical_section(Cyclist*, int, int);
void print_cyclist(Cyclist);
void print_cyclists(Cyclist*);
int lap_complete(int);
void write_cyclist(Cyclist*, int);
void erase_cyclist(Cyclist*, int);
void new_lap(Cyclist*, int);
void update_places(Cyclist*);
int input_checker(int, char **);
char get_mode(char **);
void destroy_locks_and_semaphores();
void write_log_elimination_info(Cyclist*);
void write_log_break_info(Cyclist *cyclist);
void update_timers(Cyclist*);

int main(int argc, char **argv)
{
   int cyclists, *initial_config, initial_speed;
   /*threads array. Each cyclist is a thread.*/
   pthread_t *my_threads;
   /*thread in charge of the time elapsed in the simulation*/
   pthread_t time_thread, log_thread;
   /*Thread arguments is the cyclist struct*/
   Cyclist *thread_args;

   /*Get initial information to feed the program*/
   total_cyclists = cyclists_competing = cyclists = input_checker(argc, argv);
   mode = get_mode(argv);
   if(mode == 'u' || mode == 'U') initial_speed = 50;
   else initial_speed = 25;


   /*Starting order of the cyclists is in the array initial_config[0...cyclists-1]. Each cyclist is recognized by its unique number*/
   initial_config = initial_configuration(cyclists);

   /*Threads (cyclists).*/
   my_threads = malloc(cyclists * sizeof(*my_threads));
   /*Thread args (cyclist structs)*/
   thread_args = malloc(cyclists * sizeof(Cyclist));

   /*Sets go to false. Cyclists can't start unless go is true*/
   go = 0;

   /*Sets the size of the track*/
   /*Multiplied by 2 because cyclist can move 0.5m when they are with a speed of 25km/h*/
   track_size = atoi(argv[1]);

   /*Initialize global variables related to break functionality*/
   try_to_break = total_cyclists + 1;
   update = 0;

   /*Initialize global variables related to elimination functionality*/
   already_eliminated = 0;
   if (pthread_mutex_init(&elimination_lock, NULL) != 0)
   {
      printf("\nElimination MUTEX initialization failed.\n");
      exit(1);
   }

   /*Allocates the track*/
   make_track();

   /*Now the program is ready to go*/
   printf("\nPlacing competitors...\n\n");
   sleep(1);
   make_cyclists(thread_args, initial_config, initial_speed, cyclists);
   put_cyclists_in_track(thread_args, cyclists);
   print_cyclists(thread_args);
   create_threads(cyclists, my_threads, thread_args);
   sleep(1);
   printf("\nAdjusting chronometer... ");
   sleep(3);
   if (pthread_create(&time_thread, NULL, omnium_chronometer, thread_args)) 
   {
      printf("Error creating time thread.");
      abort();
   }
   if (pthread_create(&log_thread, NULL, omnium_logger, thread_args)) 
   {
      printf("Error creating log thread.");
      abort();
   } 

   join_time_thread(time_thread);
   join_log_thread(log_thread);
   join_threads(cyclists, my_threads);
   free(initial_config);
   free(my_threads);
   free(thread_args);
   destroy_locks_and_semaphores();
   pthread_mutex_destroy(&elimination_lock);
   free(track);
   return 0;
}

/*Critical Section*/
void critical_section(Cyclist *cyclist, int old_position, int new_position)
{
   /*Lock relative to his new position*/
   pthread_mutex_lock(&track[new_position].meter_lock);
      /*If he is going to complete a lap, increments. Will eliminate the worst cyclist too.*/
      new_lap(cyclist, new_position);
      /*Writes the cyclist in the new position*/
      if(cyclist->eliminated == 'N' && cyclist->broken == 'N') write_cyclist(cyclist, new_position);
   pthread_mutex_unlock(&track[new_position].meter_lock);
   
   /*Lock relative to his old position*/
   pthread_mutex_lock(&track[old_position].meter_lock);
      /*Swap places in case of an overtake*/
      if(cyclist->eliminated == 'N' && cyclist->broken == 'N') overtake(cyclist, old_position);
      /*Releases cyclist old position*/
      erase_cyclist(cyclist, old_position);
      /*If he is eliminated, the number of cyclists in the competition is decreased*/
      if(cyclist->eliminated == 'Y' || cyclist->broken == 'Y') cyclists_competing--;
   pthread_mutex_unlock(&track[old_position].meter_lock);
}

/*If he is going to complete a new lap, do tasks relative to this*/
void new_lap(Cyclist *cyclist, int new_position)
{
   if(lap_complete(new_position)) 
   {
      /*Increments his lap*/
      (cyclist->lap)++;

      /*Eliminate the cyclist is he is the worst in the competition*/
      eliminate_cyclist(cyclist, new_position);
      write_log_elimination_info(cyclist);

      /*If he is at the first position in the race and his lap is a multiple of 4, choose a cyclist to try to break*/
      if(cyclist->lap > 1 && cyclist->place == 1 && cyclist->lap % 4 == 1) try_to_break = roll_cyclist_to_try_to_break();      

      /*See if this cyclist will break*/
      if(try_to_break == cyclist->place) break_cyclist(cyclist);

      /*Attempts to change cyclist speed (omnium_v only) */
      if(mode == 'v' || mode == 'V') cyclist->speed = roll_speed();
   }
}

void write_log_elimination_info(Cyclist *cyclist)
{
   int special_position = track_size;
   pthread_mutex_lock(&track[special_position].meter_lock);
   if(cyclist->place == cyclists_competing - 2) 
   {
      if(track[special_position].cyclist1 == NULL) { track[special_position].cyclist1 = cyclist; (track[special_position].cyclists)++; }
   }
   if(cyclist->place == cyclists_competing - 1)
   {
      if(track[special_position].cyclist2 == NULL) { track[special_position].cyclist2 = cyclist; (track[special_position].cyclists)++; }
   }
   if(cyclist->place == cyclists_competing)
   {
      if(track[special_position].cyclist3 == NULL) { track[special_position].cyclist3 = cyclist; (track[special_position].cyclists)++; }
   }
   pthread_mutex_unlock(&track[special_position].meter_lock);
}

void write_log_break_info(Cyclist *cyclist)
{
   int special_position = track_size;
   pthread_mutex_lock(&track[special_position].meter_lock);
      if(track[special_position].cyclist4 == NULL) { track[special_position].cyclist4 = cyclist; (track[special_position].cyclists)++; }
   pthread_mutex_unlock(&track[special_position].meter_lock);
}

/*Attempts to break the cyclist*/
void break_cyclist(Cyclist *cyclist)
{
   /*The remaining last 3 cyclists are immune to break attempts*/
   if(cyclist->eliminated == 'N' && cyclists_competing > 3)
   {
      /*1% chance to break the cyclist*/
      if(rand() % 100 == 0) 
      { 
         mark_cyclist(cyclist, 'B');
         /*He broke. He is now in the last place of this lap*/
         cyclist->place = cyclists_competing;
         /*Calls for update cyclists places because someone broke*/
         update = 1; 
         /*Write in the special position of the track this cyclist will break*/
         write_log_break_info(cyclist);
      }
   }
}

/*Swap cyclists places in the case of an overtaking.*/
void overtake(Cyclist *cyclist, int position)
{
   int temp;
   /*Must update places*/
   if(track[position].cyclists > 0)
   {
      if(track[position].cyclist1 != NULL && track[position].cyclist1 != cyclist)
      {
         if(((*track[position].cyclist1).place < cyclist->place) && ((*track[position].cyclist1).lap <= cyclist->lap))
         {
            temp = cyclist->place;
            cyclist->place = (*track[position].cyclist1).place;
            (*track[position].cyclist1).place = temp;
         }
      }
      if(track[position].cyclist2 != NULL && track[position].cyclist2 != cyclist)
      {
         if(((*track[position].cyclist2).place < cyclist->place) && ((*track[position].cyclist2).lap <= cyclist->lap))
         {
            temp = cyclist->place;
            cyclist->place = (*track[position].cyclist2).place;
            (*track[position].cyclist2).place = temp;
         }
      }
      if(track[position].cyclist3 != NULL && track[position].cyclist3 != cyclist)
      {
         if(((*track[position].cyclist3).place < cyclist->place) && ((*track[position].cyclist3).lap <= cyclist->lap))
         {
            temp = cyclist->place;
            cyclist->place = (*track[position].cyclist3).place;
            (*track[position].cyclist3).place = temp;
         }
      }
      if(track[position].cyclist4 != NULL && track[position].cyclist4 != cyclist)
      {
         if(((*track[position].cyclist4).place < cyclist->place) && ((*track[position].cyclist4).lap <= cyclist->lap))
         {
            temp = cyclist->place;
            cyclist->place = (*track[position].cyclist4).place;
            (*track[position].cyclist4).place = temp;
         }
      }  
   }
}

/*Eliminates the worst cyclist of the lap*/
void eliminate_cyclist(Cyclist *cyclist, int new_position)
{
   /*Confirms positions. Did he really crossed the line and it's the worst cyclist in the race?*/
   if((new_position == 0) && (cyclist->place == cyclists_competing))
   {
      pthread_mutex_lock(&elimination_lock);
         if(already_eliminated == 0)
         {
            already_eliminated = 1;
            mark_cyclist(cyclist, 'E');
         }
      pthread_mutex_unlock(&elimination_lock);
   }
}

/*Marks the cyclist to be eliminated from the competition*/
void mark_cyclist(Cyclist *cyclist, char mark)
{
   /*Marks the cyclists to eliminate him later*/
   if(mark == 'E') cyclist->eliminated = 'Y';
   else /*mark == 'B'*/ cyclist->broken = 'Y';
}

/*Checks is the cyclist in this position will complete a new lap*/
int lap_complete(int position)
{
   if(0 == position) return 1;
   return 0;
}

/*Attempts to change cyclist speed*/
int roll_speed() 
{
  return ((rand() % 2) + 1) * 25; 
}

/*Rolls a number, where this number is the position of a cyclist in the race. This number determines who will suffer a break attempt*/
int roll_cyclist_to_try_to_break()
{
   return ((rand() % cyclists_competing) + 1);  
}

/*Writes the cyclists in the new track position*/
void write_cyclist(Cyclist *cyclist, int new_position)
{
   if(track[new_position].cyclist1 == NULL) track[new_position].cyclist1 = cyclist;
   else if(track[new_position].cyclist2 == NULL) track[new_position].cyclist2 = cyclist;
   else if(track[new_position].cyclist3 == NULL) track[new_position].cyclist3 = cyclist;
   else track[new_position].cyclist4 = cyclist;
   (track[new_position].cyclists)++;
   if(track[new_position].cyclists > MAX_CYCLISTS) 
   {
      printf("\nError. Found more than 4 cyclists in track[%d].\n", new_position);
      exit(0);
   }
   /*Assigns the new position to the cyclist*/
   cyclist->position = new_position;
}

/*Erases the cyclists from his old track position*/
void erase_cyclist(Cyclist *cyclist, int old_position)
{
   if(track[old_position].cyclist1 == cyclist) track[old_position].cyclist1 = NULL;
   else if(track[old_position].cyclist2 == cyclist) track[old_position].cyclist2 = NULL;
   else if(track[old_position].cyclist3 == cyclist) track[old_position].cyclist3 = NULL;
   else track[old_position].cyclist4 = NULL;
   (track[old_position].cyclists)--;
   if(track[old_position].cyclists < NO_CYCLISTS) 
   {
      printf("\nError. Found negative number of cyclists in track[%d].\n", old_position);
      exit(0);
   }
}

/*Decides the next position considering speed and index in track*/
int decide_new_position(Cyclist *cyclist)
{
   /*Updates half field of the cyclist*/
   if(cyclist->speed == 25)
   {
      if(cyclist->half == 0) cyclist->half = 0.5;
      else cyclist->half = 0;
   }

   if((cyclist->speed == 25 && cyclist->half == 0) || cyclist->speed == 50)
   {
      if(cyclist->position == track_size-1) return 0;
      return cyclist->position + 1;
   }
   return cyclist->position;
}

/*Confirms if the cyclists is out*/
int disqualified(Cyclist *cyclist)
{
   if(cyclist->eliminated == 'Y' || cyclist->broken == 'Y') return 1;
   return 0;
}

/*Broadcasts, announcing broken, and eliminated cyclists and the winner of the race*/
void broadcast(Cyclist *cyclist)
{
   int sec = cyclist->cyclist_timer / CLOCKS_PER_SEC;
   if(cyclist->eliminated == 'Y')
      printf("\n*****************************\nThe cyclist %d (%p) has been ELIMINATED (time: %ds). Place: %d\n*****************************\n", cyclist->number, (void*)cyclist, sec, cyclist->place);
   else if(cyclist->broken == 'Y')
      printf("\n*****************************\nThe cyclist %d (%p) has BROKEN (time: %ds). Place: %d\n*****************************\n", cyclist->number, (void*)cyclist, sec, cyclist->place);
   else
      printf("\n*****************************\nThe cyclist %d (%p) has WON THE RACE (time: %ds). Place: %d\n*****************************\n", cyclist->number, (void*)cyclist, sec, cyclist->place);
}

/*Omnium race function. Each thread is representing a cyclist in omnium*/
void *omnium(void *args)
{
   int new_position, old_position;
   Cyclist *cyclist = ((Cyclist*) args);

   old_position = cyclist->position;
   while(!go) continue;

   for(new_position = decide_new_position(cyclist); cyclists_competing != 1; new_position = decide_new_position(cyclist)) 
   {
      if(old_position != new_position) 
      {
         sem_wait(&track[new_position].mutex);
         critical_section(cyclist, old_position, new_position);
         sem_post(&track[old_position].mutex);
         old_position = new_position;
      }
      if(disqualified(cyclist) == 1) break;
      await(72000000); /*Each cyclist make a move every 0.72ms. 1m or 0.5m, depending on his speed*/
   }

   sem_post(&track[new_position].mutex);
   broadcast(cyclist);

   pthread_mutex_lock(&elimination_lock);
      already_eliminated = 0;
   pthread_mutex_unlock(&elimination_lock);

   return NULL;
}

/*Runs the chronometer. The chronometer also prints on the screen the information about the race.*/
void *omnium_chronometer(void *args)
{
   int cycles = 0;
   Cyclist *all_cyclists = args;

   /*Race will start. After countdown(), all cyclist threads will be unlocked.*/
   countdown();
   /*RELEASE THE CYCLISTS!*/
   /*Race chronometer*/
   start_timer = clock();
   
   /*Time thread will run until we have just 1 cyclist competing*/
   while(cyclists_competing != 1)
   {
      /*Simulation timer, counted in cycles of 0.72ms*/
      await(72000000);
      /*Update places in case of a break*/
      update_timers(all_cyclists);
      if(update == 1) update_places(all_cyclists);
      /*DEBUG MODE*/
      if(mode == 'U' || mode == 'V') 
      { 
         print_cyclists(all_cyclists); 
         if(cycles % 20 == 0) cycles = 0; 
         cycles++;
      }
   }
   return NULL;
}

void *omnium_logger(void *args)
{
   FILE *pfile;
   int i, lap = 1, special_position = track_size, order[3];
   char str[256];
   Cyclist *all_cyclists = args;
   
   pfile = fopen("output/race.log", "w");

   /*Writes the first lap in the output*/
   fputs("OMNIUM LOG (Mode = ", pfile); fputc(mode, pfile); fputs("):\n\n", pfile);

   while(cyclists_competing != 1)
   {
      pthread_mutex_lock(&track[special_position].meter_lock);
      /*Writes info in the log: eliminated cyclists and the remaining last 2 cyclists. Also, writed next lap info.*/
      if(track[special_position].cyclist1 != NULL && track[special_position].cyclist2 != NULL && track[special_position].cyclist3 != NULL)
      {
         int eliminated_place = (*track[special_position].cyclist3).place; 

         fputs("LOSERS OF LAP ", pfile);
         sprintf(str, "%d", lap++); fputs(str, pfile);
         fputs(":\n", pfile);

         fputs("Cyclist #", pfile);
         sprintf(str, "%d", (*track[special_position].cyclist1).number); fputs(str, pfile);
         fputs(" has terminated this lap in position ", pfile);
         sprintf(str, "%d", eliminated_place - 2); fputs(str, pfile);
         fputs(" of ", pfile);
         sprintf(str, "%d", total_cyclists); fputs(str, pfile); 
         fputs(".\n", pfile);

         fputs("Cyclist #", pfile);
         sprintf(str, "%d", (*track[special_position].cyclist2).number); fputs(str, pfile);
         fputs(" has terminated this lap in position ", pfile);
         sprintf(str, "%d", eliminated_place - 1); fputs(str, pfile);
         fputs(" of ", pfile);
         sprintf(str, "%d", total_cyclists); fputs(str, pfile); 
         fputs(".\n", pfile);

         fputs("Cyclist #", pfile);
         sprintf(str, "%d", (*track[special_position].cyclist3).number); fputs(str, pfile);
         fputs(" has terminated this lap in position ", pfile);
         sprintf(str, "%d", eliminated_place); fputs(str, pfile);
         fputs(" of ", pfile);
         sprintf(str, "%d", total_cyclists); fputs(str, pfile); 
         fputs(". -> ELIMINATED.\n", pfile);

         track[special_position].cyclist1 = NULL;
         track[special_position].cyclist2 = NULL;
         track[special_position].cyclist3 = NULL;
         track[special_position].cyclists -= 3;  
      }
      /*Writes info in the log: broken cyclist*/
      if(track[special_position].cyclist4 != NULL)
      {
         fputs("KNOCKED OUT IN LAP ", pfile);
         sprintf(str, "%d", lap); fputs(str, pfile);
         fputs(":\n", pfile);

         fputs("Cyclist #", pfile);
         sprintf(str, "%d", (*track[special_position].cyclist4).number); fputs(str, pfile);
         fputs(" has been knocked out. His final standing is ", pfile);
         sprintf(str, "%d", (*track[special_position].cyclist4).place); fputs(str, pfile);
         fputs(" of ", pfile);
         sprintf(str, "%d", total_cyclists); fputs(str, pfile); 
         fputs(". -> BROKEN.\n", pfile);

         track[special_position].cyclist4 = NULL;
         (track[special_position].cyclists)--;
      }
      pthread_mutex_unlock(&track[special_position].meter_lock);
   }

   for(i = 0; i < total_cyclists; i++)
   {
      if(all_cyclists[i].place == 1) order[0] = i;
      if(all_cyclists[i].place == 2) order[1] = i;
      if(all_cyclists[i].place == 3) order[2] = i;
   }

   /*Writes the winners*/
   fputs("\n\nOMNIUM WINNERS:\n", pfile);
   fputs("\n1st place: Cyclist #", pfile); sprintf(str, "%d", all_cyclists[order[0]].number); fputs(str, pfile); fputc('.', pfile);
   fputs("\n2nd place: Cyclist #", pfile); sprintf(str, "%d", all_cyclists[order[1]].number); fputs(str, pfile); fputc('.', pfile);
   fputs("\n3rd place: Cyclist #", pfile); sprintf(str, "%d", all_cyclists[order[2]].number); fputs(str, pfile); fputc('.', pfile);

   fclose(pfile);

   return NULL;
}


/*Function to wait x ms.*/
void await(int x)
{
   struct timespec tim, tim2;
   tim.tv_sec = tim2.tv_sec = 0;
   tim.tv_nsec = tim2.tv_nsec = x;

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

/*Allocates the track*/
void make_track()
{
   int i;
   track = malloc((track_size + 1) * sizeof(Meter));
   /*Note: the last position of track is used ONLY by the logger. It is not a real meter. Is just contains information to write the output*/
   for(i = 0; i <= track_size; i++)
   {
      track[i].cyclist1 = NULL;
      track[i].cyclist2 = NULL;
      track[i].cyclist3 = NULL;
      track[i].cyclist4 = NULL;
      track[i].cyclists = 0;
      if (pthread_mutex_init(&track[i].meter_lock, NULL) != 0)
      {
         printf("\n mutex init failed\n");
         exit(1);
      }
      if (sem_init(&track[i].mutex, 0, 4)) 
      {
         printf("Erro ao criar o semáforo :(\n");
         exit(2);
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
      thread_args[i].position = i; /*At the start of the race, all cyclists starts with 1m space of each other independent of the mode*/
      thread_args[i].place = cyclists - i;
      thread_args[i].speed = initial_speed;
      thread_args[i].lap = 1; /*first lap*/
      thread_args[i].eliminated = 'N';
      thread_args[i].broken = 'N';
      thread_args[i].half = 0;
   }
}

/*Assigns cyclists to track positions in the beginning of the simulation*/
void put_cyclists_in_track(Cyclist *thread_args, int cyclists)
{
   int i;
   for(i = 0; i < cyclists; i++)
   {
      track[i].cyclist1 = &thread_args[i];
      track[i].cyclists = 1;
      sem_wait(&track[i].mutex);
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

/*Prints cyclist information*/
void print_cyclist(Cyclist cyclist)
{
   printf("Cyclist #%d | Track Position:  %.1fm | Place: %d | Speed: %d | Lap: %d\n", cyclist.number, cyclist.position + cyclist.half, cyclist.place, cyclist.speed, cyclist.lap);
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

/*Function to join the log thread*/
void join_log_thread(pthread_t log_thread)
{
  if (pthread_join(log_thread, NULL)) 
  {
      printf("Error joining log thread.");
      abort();
  }
}

/*Function to create all Cyclists threads*/
void create_threads(int cyclists, pthread_t *my_threads, Cyclist *thread_args)
{
   int i;
   for(i = 0; i < cyclists; i++)
   {
      if (pthread_create(&my_threads[i], NULL, omnium, &thread_args[i])) 
      {
         printf("Error creating thread.");
         abort();
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

/*Gets the selected mode for the simulation*/
char get_mode(char **argv)
{
   if(strcasecmp(argv[3], "u") != 0 && strcasecmp(argv[3], "v") != 0) {
      printf("Mode argument is expected to be 'u' or 'v'.\n");
      exit(-1);
   }
   return argv[3][0];
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

/*Prints cyclists*/
void print_cyclists(Cyclist *all_cyclists)
{
   int i = 0;
   while(i < total_cyclists) print_cyclist(all_cyclists[i++]);
   printf("\n");
}

/*Updates cyclists places if a cyclist broke*/
void update_places(Cyclist *all_cyclists)
{
   int i = 0;
   while(i < total_cyclists)
   {
      /*Updates cyclists positions in case someone has broken*/
      if(try_to_break < all_cyclists[i].place && (all_cyclists[i].broken == 'N' && all_cyclists[i].eliminated == 'N')) (all_cyclists[i].place)--;
      i++;
   }
   try_to_break = total_cyclists + 1;
   update = 0;
}

void destroy_locks_and_semaphores()
{
   int i = 0;
   for(i = 0; i <= track_size; i++)
   {
      pthread_mutex_destroy(&track[i].meter_lock);
      sem_destroy(&track[i].mutex);
   }
}

void update_timers(Cyclist *all_cyclists)
{
   int i = 0;
   for(i = 0; i < total_cyclists; i++) all_cyclists[i].cyclist_timer = clock() - start_timer;
}