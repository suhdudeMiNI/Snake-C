//Oswiadczam, ze niniejsza praca stanowiaca 
//podstawe do uznania osiagniecia efektow uczenia sie
//z przedmitu SOP1 zostala wykonana przeze mnie samodzielnie.
//Maksim Matchenia
//308818

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <stddef.h>
#include <pthread.h>
#include <math.h>
#include <ctype.h>

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGINT),\
		     		     exit(EXIT_FAILURE))

#define min(a, b) (a < b? a : b)

#define MAX_SNAKES 10
#define MAX_PATH_LEN 20
#define MAX_MAP 100
#define MAX_INPT 20

typedef struct coord
{
    int x;
    int y;
} coord_t;

typedef struct snake
{
    pthread_t id;
    char (*field)[MAX_MAP];
    pthread_mutex_t* field_mx;
    char letter;
    int speed;
    int len;
    coord_t segments[MAX_MAP * MAX_MAP];
    int size_x;
    int size_y;
} snake_t;

typedef struct options
{
    int size_x;
    int size_y;
    char filePath[MAX_PATH_LEN]; 
    snake_t snakes[MAX_SNAKES];
    int NumSnakes;
    pthread_mutex_t* options_lock;

} opt_t;

typedef struct map
{
    int size_x;
    int size_y;
    char field[MAX_MAP][MAX_MAP]; 
} map_t;



typedef struct screen
{
    pthread_t id;
    int size_x;
    int size_y;
    char (*field)[MAX_MAP];
    pthread_mutex_t* field_mx;
} screen_t;

typedef struct zapis
{
    int NumSnakes;
    int size_x;
    int size_y;
    char field[MAX_MAP][MAX_MAP];
    char filePath[MAX_PATH_LEN];
    snake_t snakes[MAX_SNAKES];
    pthread_t id;
} zapis_t;

void usage();
void ReadArgs(int, char**, opt_t*, map_t*);
void ClearMap(map_t*);
void* ReadKey(void*);
void* SnakeWork(void*);
void* Show(void*);
void* Save(void*);
void* Load(void*);

int main(int argc, char** argv)
{
    srand(time(NULL));
    opt_t opt;
    map_t karta;
	ReadArgs(argc, argv, &opt, &karta);
    pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
    opt.options_lock = &mx;
   
    sigset_t mask, oldmask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigaddset(&mask, SIGUSR2);
	sigaddset(&mask, SIGINT);
	if (sigprocmask(SIG_BLOCK, &mask, &oldmask)) ERR("error");

    pthread_attr_t thread_attr;
	if (pthread_attr_init(&thread_attr)) ERR("pthread_attr_init");
	if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED)) ERR("pthread_attr_setdetach_state");

    pthread_mutex_t field_mx = PTHREAD_MUTEX_INITIALIZER;
    for(int i = 0; i < opt.NumSnakes; i++)
    {
        opt.snakes[i].field = karta.field;
        opt.snakes[i].field_mx = &field_mx;
        opt.snakes[i].size_x = karta.size_x;
        opt.snakes[i].size_y = karta.size_y;
        if (pthread_create(&(opt.snakes[i].id), &thread_attr, SnakeWork, (void*)&opt.snakes[i])) ERR("pthread_create");
    }

    pthread_t key_id;
    if (pthread_create(&key_id, &thread_attr, ReadKey, (void*)&opt)) ERR("phread_create");

    screen_t scr;
    scr.size_x = karta.size_x;
    scr.size_y = karta.size_y;
    scr.field = karta.field;
    scr.field_mx = &field_mx;

    zapis_t zap;
    
    zap.size_x = karta.size_x;
    zap.size_y = karta.size_y;
    strcpy(zap.filePath, opt.filePath);

    int signo;
    bool running = true;
	while (running) {
		if (sigwait(&mask, &signo)) ERR("sigwait failed.");
        switch(signo)
        {
            case SIGINT:
                for (int i = 0; i < opt.NumSnakes; i++)
                    pthread_kill(opt.snakes[i].id, SIGINT);
                running = false;
                break;
            
            case SIGUSR1:
                pthread_create(&scr.id, &thread_attr, Show, (void*)&scr);
                break;

            case SIGUSR2:
                if (pthread_mutex_lock(opt.options_lock)) ERR("mutex_lock");
                zap.NumSnakes = opt.NumSnakes;
                for(int i = 0; i < zap.NumSnakes; i++)
                {
                    zap.snakes[i] = opt.snakes[i];
                    for(int j = 0; j < zap.snakes[i].len; j++)
                    {
                        zap.snakes[i].segments[j] = opt.snakes[i].segments[j];
                    }
                }
                if (pthread_mutex_unlock(opt.options_lock)) ERR("mutex_unlock");
                if (pthread_mutex_lock(&field_mx)) ERR("mutex_lock");
                for(int i = 0; i< MAX_MAP; i++)
                {
                    for(int j = 0; j < MAX_MAP; j++)
                    {
                        zap.field[i][j] = karta.field[i][j];
                    }
                }
                if (pthread_mutex_unlock(&field_mx)) ERR("mutex_unlock");

                if (pthread_create(&zap.id, &thread_attr, Save, (void*)&zap)) ERR("pthread_create");
                break;
        }
		if (signo == SIGINT) break;
	}
}

void usage() 
{
	fprintf(stderr, "USAGE: ./tsnake [-x Xdim = 20] [-y Ydim=10] [-f file = $SNAKEFILE] c1:s1 [c2:s2 ...]\n");
	fprintf(stderr, "-x  -   rozmiar pola(wsp. x)\n");
	fprintf(stderr, "-y  -   rozmiar pola(wsp. y)\n");
	fprintf(stderr, "-f  -   sciezka zapisywania wynikow gry\n");
	fprintf(stderr, "-c1 -   litera Glowy weza\n");
	fprintf(stderr, "-s1 -   predkosc weza\n");
	exit(EXIT_FAILURE);
}

void ReadArgs(int argc, char** argv, opt_t* opt, map_t* karta)
{
    opt->size_x = 20;
    opt->size_y = 10;
    opt->filePath[0] = '\0';

	int entry;

	while ((entry = getopt(argc, argv, "x:y:f:")) != -1)
	{
		switch (entry)
		{
		case 'x':
			opt->size_x = atoi(optarg);;
			break;
		case 'y':
			opt->size_y = atoi(optarg);
			break;
		case 'f':
			strcpy(opt->filePath, optarg);            
			break;
		default:
			usage();
		}
	}

    char *env = getenv("SNAKEFILE");
    if (strcmp(opt->filePath, "") == 0) {
        if (env) strcpy(opt->filePath, env);  
    }

	if (argc < optind + 1) usage();

    opt->NumSnakes = 0;
	for (int i = optind; i < argc; i++){
        opt->snakes[opt->NumSnakes].letter = tolower(*strtok(argv[i], ":"));
        opt->snakes[opt->NumSnakes].speed = atoi(strtok(NULL, ":"));
        bool alr = false;
        for(int j = 0; j < opt->NumSnakes; j++){
            if(opt->snakes[opt->NumSnakes].letter == opt->snakes[j].letter){
                alr = true;
                break;
            }
        }
        if(alr == false) opt->NumSnakes++;
    }

    karta->size_x = opt->size_x;
    karta->size_y = opt->size_y;
    
   ClearMap(karta);




    if (access(opt->filePath, F_OK) == 0)
    {
        zapis_t* zap;
        pthread_t id;
        if (pthread_create(&id, NULL, Load, (void*)opt->filePath)) ERR("pthrad_create");
        if (pthread_join(id, (void**)&zap)) ERR("phread_join");

        opt->size_x = zap->size_x;
        opt->size_y = zap->size_y;

        if(opt->NumSnakes != zap->NumSnakes) ERR("Blad wprowadzonych danych");

        for(int i = 0; i < zap->NumSnakes; i++)
        {
            bool found = false;
            for (int j = 0; j < opt->NumSnakes; j++)
            {
                if(zap->snakes[i].letter == opt->snakes[j].letter && zap->snakes[i].speed == opt->snakes[j].speed)
                {
                    opt->snakes[j].len = zap->snakes[i].len;
                    for (int k = 0; k < opt->snakes[j].len; k++)
                        opt->snakes[j].segments[k] = zap->snakes[i].segments[k];
                    found = true;
                    break;
                }
            }
            
            if(!found) ERR("Blad wprowadzonych danych");
        }
        
        for(int i = 0; i < zap->size_y; i++ )
        {
            for (int j = 0; j < zap->size_x; j ++)
            {
                karta->field[i][j] = zap->field[i][j];
            }        
        }
    }

    else{
        
        if(strcmp(opt->filePath, "") == 0)
        {
            printf("Sciezka zapisywania gry nie jest podana. Nie ma mozliwosci zapisac wyniki gry\n");
        }
    }


}

void ClearMap(map_t* mp)
{
    for (int i = 0; i < mp->size_y; i++)
        for (int j = 0; j < mp->size_x; j++)
            mp->field[i][j] = ' ';
}

void* ReadKey(void* opcje)
{
    opt_t* opt = (opt_t*) opcje;
	char inpt[MAX_INPT + 2];
	char buf[6];
	while (fgets(inpt, MAX_INPT + 2, stdin) != NULL)
	{
		if (!strcmp(inpt, "exit\n")) kill(0, SIGINT);
		else if (!strcmp(inpt, "show\n")) kill(0, SIGUSR1);
		else if (!strcmp(inpt, "save\n")) kill(0, SIGUSR2);
		else
		{
			strncpy(buf, inpt, sizeof(char) * 5);
			buf[5] = '\0';
			if (!strcmp(buf, "spawn"))
			{
                if (pthread_mutex_lock(opt->options_lock)) ERR("mutex_lock");
                char letter = tolower(inpt[6]);
                int speed = atoi(inpt + 8);
                bool alr = false;
                for (int i = 0; i < opt->NumSnakes; i++)
                    if (opt->snakes[i].letter == letter)
                    {
                        alr = true;
                        break;
                    }
                if (!alr)
                {
                    opt->snakes[opt->NumSnakes].letter = letter;
                    opt->snakes[opt->NumSnakes].speed = speed;
                    opt->snakes[opt->NumSnakes].field = opt->snakes[0].field;
                    opt->snakes[opt->NumSnakes].field_mx = opt->snakes[0].field_mx;
                    opt->snakes[opt->NumSnakes].size_x = opt->size_x;
                    opt->snakes[opt->NumSnakes].size_y = opt->size_y;
                    pthread_create(&(opt->snakes[opt->NumSnakes].id), NULL, SnakeWork, (void*)&opt->snakes[opt->NumSnakes]);
                    opt->NumSnakes++;
                }
                if (pthread_mutex_unlock(opt->options_lock)) ERR("mutex_unlock");
			}
		}
	}
	return NULL;
}

void* SnakeWork(void* waz)
{
    snake_t* zmeja = (snake_t*) waz;

    coord_t buf;
    coord_t all_food[MAX_SNAKES];
    coord_t aim;
    int food_num = 0;

    if (zmeja->len < 1)
    {
        zmeja->len = 1;
        if (pthread_mutex_lock(zmeja->field_mx)) ERR("mutex_lock");
    do
    {
        buf.x = rand()%zmeja->size_x;
        buf.y = rand()%zmeja->size_y;
    } while (zmeja->field[buf.y][buf.x] != ' ');
    zmeja->field[buf.y][buf.x] = toupper(zmeja->letter);
    zmeja->segments[0] = buf;
    do
    {
        buf.x = rand()%zmeja->size_x;
        buf.y = rand()%zmeja->size_y;
    } while (zmeja->field[buf.y][buf.x] != ' ');
    zmeja->field[buf.y][buf.x] = '0';
    for (int i = 0; i < zmeja->size_y; i++)
        for (int j = 0; j < zmeja->size_x; j++){
            if (zmeja->field[i][j] == '0'){
            
                buf.x = j;
                buf.y = i;
                all_food[food_num++] = buf;
            }
        }
    aim = all_food[rand()%food_num];
    if (pthread_mutex_unlock(zmeja->field_mx)) ERR("mutex_unlock");
    }

    float fromUp, fromDown, fromLeft, fromRight;
    float fromMin;

    while (true)
    {
        if (aim.x == zmeja->segments[0].x && aim.y == zmeja->segments[0].y)
        {
            if (pthread_mutex_lock(zmeja->field_mx)) ERR("mutex_lock");
            food_num = 0;
            for (int i = 0; i < zmeja->size_y; i++)
                for (int j = 0; j < zmeja->size_x; j++)
                    if (zmeja->field[i][j] == '0')
                    {
                        buf.x = j;
                        buf.y = i;
                        all_food[food_num++] = buf;
                    }
            aim = all_food[rand()%food_num];
            if (pthread_mutex_unlock(zmeja->field_mx)) ERR("mutex_unlock");
        }

        fromUp = fromDown = fromLeft = fromRight = MAX_MAP*sqrt(2);
        
        if (pthread_mutex_lock(zmeja->field_mx)) ERR("mutex_lock");
        if (zmeja->segments[0].y > 0 && (zmeja->field[zmeja->segments[0].y - 1][zmeja->segments[0].x] == '0' || zmeja->field[zmeja->segments[0].y - 1][zmeja->segments[0].x] == ' '))
            fromUp = sqrt(pow(aim.x - zmeja->segments[0].x, 2) + pow(aim.y - zmeja->segments[0].y + 1, 2));
        if (zmeja->segments[0].y < zmeja->size_y-1 && (zmeja->field[zmeja->segments[0].y + 1][zmeja->segments[0].x] == '0' || zmeja->field[zmeja->segments[0].y + 1][zmeja->segments[0].x] == ' '))
            fromDown = sqrt(pow(aim.x - zmeja->segments[0].x, 2) + pow(aim.y - zmeja->segments[0].y - 1, 2));
        if (zmeja->segments[0].x > 0 && (zmeja->field[zmeja->segments[0].y][zmeja->segments[0].x - 1] == '0' || zmeja->field[zmeja->segments[0].y][zmeja->segments[0].x - 1] == ' '))
            fromLeft = sqrt(pow(aim.x - zmeja->segments[0].x + 1, 2) + pow(aim.y - zmeja->segments[0].y, 2));
        if (zmeja->segments[0].x < zmeja->size_x-1 && (zmeja->field[zmeja->segments[0].y][zmeja->segments[0].x + 1] == '0' || zmeja->field[zmeja->segments[0].y][zmeja->segments[0].x + 1] == ' '))
            fromRight = sqrt(pow(aim.x - zmeja->segments[0].x - 1, 2) + pow(aim.y - zmeja->segments[0].y, 2));
        
        fromMin = min(fromUp, min(fromDown, min(fromLeft, fromRight)));
        if (fromMin < MAX_MAP*sqrt(2) - 1)
        {
            for (int i = zmeja->len+1; i > 0; i--)
                zmeja->segments[i] = zmeja->segments[i-1];
            if (fromUp == fromMin)
            {
                zmeja->segments[0].y--;
            }
            else if (fromDown == fromMin)
            {
                zmeja->segments[0].y++;
            }
            else if (fromLeft == fromMin)
            {
                zmeja->segments[0].x--;
            }
            else
            {
                zmeja->segments[0].x++;
            }
            if (zmeja->field[zmeja->segments[0].y][zmeja->segments[0].x] == '0')
            {
                do
                {
                    buf.x = rand()%zmeja->size_x;
                    buf.y = rand()%zmeja->size_y;
                } while (zmeja->field[buf.y][buf.x] != ' ');
                zmeja->field[buf.y][buf.x] = '0';
                zmeja->len++;
            }
            for (int i = 1; i < zmeja->len; i++)
                zmeja->field[zmeja->segments[i].y][zmeja->segments[i].x] = zmeja->letter;
            zmeja->field[zmeja->segments[zmeja->len].y][zmeja->segments[zmeja->len].x] = ' ';

            zmeja->field[zmeja->segments[0].y][zmeja->segments[0].x] = toupper(zmeja->letter);
        }
        if (pthread_mutex_unlock(zmeja->field_mx)) ERR("mutex_unlock");
        usleep(zmeja->speed * 1000);
    }

    zmeja->segments[0] = buf;

    return NULL;
}

void* Show(void* mapa)
{
    screen_t* karta = (screen_t*) mapa;
    if (pthread_mutex_lock(karta->field_mx)) ERR("mutex_lock");
    printf("x");
    for (int i = 1; i < karta->size_x+1; i++)
        printf("-");
    printf("x\n");

    for (int i = 1; i < karta->size_y+1; i++)
    {
        printf("|");
        for (int j = 1; j < karta->size_x+1; j++)
            printf("%c", karta->field[i-1][j-1]);
        printf("|\n");
    }
    printf("x");
    for (int i = 1; i < karta->size_x+1; i++)
        printf("-");
    printf("x\n");
    if (pthread_mutex_unlock(karta->field_mx)) ERR("mutex_lock");
    return NULL;
}

void* Save(void* sohr)
{
    zapis_t* zap = (zapis_t*) sohr;
    if(strcmp(zap->filePath, "") == 0) ERR("Nazwa pliku nie jest podana");

    int file_p;
	if ((file_p = open(zap->filePath, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1) ERR("open");

    write(file_p, zap, 3*sizeof(int) + MAX_MAP*MAX_MAP*sizeof(char));
    for(int i = 0; i < zap->NumSnakes; i++)
    {
        write(file_p, &zap->snakes[i].letter, sizeof(char));
        write(file_p, &zap->snakes[i].speed, sizeof(int));
        write(file_p, &zap->snakes[i].len, sizeof(int));
        for(int j = 0; j < zap->snakes[i].len; j++)
        {
            write(file_p, &zap->snakes[i].segments[j], sizeof(coord_t));
        }
    }
    if (close(file_p)) ERR("close");
    return NULL;
}

void* Load(void* lod)
{
    char* filePath = (char*) lod;
    if (access(filePath, F_OK) != 0) ERR("PLik nie istnieje");

    int file_p;
	if ((file_p = open(filePath, O_RDONLY)) == -1) ERR("open");

    zapis_t* zap = (zapis_t*) malloc(sizeof(zapis_t));
    read(file_p, zap, 3*sizeof(int) + MAX_MAP*MAX_MAP*sizeof(char));
    for(int i = 0; i < zap->NumSnakes; i++)
    {
        read(file_p, &zap->snakes[i].letter, sizeof(char));
        read(file_p, &zap->snakes[i].speed, sizeof(int));
        read(file_p, &zap->snakes[i].len, sizeof(int));
        for(int j = 0; j < zap->snakes[i].len; j++)
        {
            read(file_p, &zap->snakes[i].segments[j], sizeof(coord_t));
        }
    }
    if (close(file_p)) ERR("close");
    return (void*)zap;
}
