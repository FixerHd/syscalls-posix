#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
extern int optind;
extern int errno;
struct dirent **ficheros;
struct stat *fichstat;
int seconds = 1;
int num_ficheros;
char dir_name[256] = ".";
char log_file[256] = "/tmp/watchdir.log";

// Función encargada de encontrar un fichero por su nodo i en la lista de ficheros y devuelve su posición en la lista, si no se encuentra devolverá -1
int find_fich(ino_t ino, struct dirent **lista, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (lista[i]->d_ino == ino)
        {
            return i;
        }
    }
    return -1;
}

// Función que se dedica a ordenar los ficheros y directorios por su nodo i
int sort(const struct dirent **a, const struct dirent **b)
{
    if ((*a)->d_ino < (*b)->d_ino)
    {
        return -1;
    }
    else if ((*a)->d_ino > (*b)->d_ino)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// Función que filtra los directorios que no sean el directorio actual o el padre
int filter(const struct dirent *direc)
{
    if (strncmp(direc->d_name, "..", 2) == 0 || strncmp(direc->d_name, ".", 1) == 0)
    {
        return 0;
    }
    else
        return 1;
}

// Función encargada de modificar el fichero log cuando se crea un nuevo fichero
void fich_creation(char *name, FILE *log)
{
    if (log)
    {
        fprintf(log, "Creation: %s\n", name);
    }
}

// Función encargada de modificar el fichero log cuando se modifica el tamaño de
// un fichero
void fich_size(char *name, off_t old_size, off_t new_size, FILE *log)
{
    if (log)
    {
        fprintf(log, "UpdateSize: %s: %ld -> %ld\n", name, old_size, new_size);
    }
}
// Función encargada de modificar el fichero log cuando se cambia la fecha de modificacion
// de un fichero
void fich_date(char *name, time_t mtim1, time_t mtim2, FILE *log)
{
    if (log)
    {
        struct tm *tiempo1;
        struct tm *tiempo2;
        char tiempo_formateado1[20];
        char tiempo_formateado2[20];
        time(&mtim1);
        time(&mtim2);
        tiempo1 = localtime(&mtim1);
        tiempo2 = localtime(&mtim2);
        strftime(tiempo_formateado1, sizeof(tiempo_formateado1), "%Y-%m-%d %H:%M:%S", tiempo1);
        strftime(tiempo_formateado2, sizeof(tiempo_formateado2), "%Y-%m-%d %H:%M:%S", tiempo2);
        fprintf(log, "UpdateMtim: %s: %s -> %s\n", name, tiempo_formateado1, tiempo_formateado2);
    }
}
// Función encargada de modificar el fichero log cuando modifica el nombre de un fichero
void fich_name(char *name, char *old_name, FILE *log)
{
    if (log)
    {
        fprintf(log, "UpdateName: %s -> %s\n", old_name, name);
    }
}

// Función encargada de modificar el fichero log cuando se elimina un fichero
void fich_delete(char *name, FILE *log)
{
    if (log)
    {
        fprintf(log, "Deletion: %s\n", name);
    }
}

// Función encargada de buscar los cambios producidos en el directorio
void monitordirec()
{
    struct dirent **lista;
    struct stat *listat;
    int n;
    n = scandir(dir_name, &lista, filter, sort);
    if (n < 0)
    {
        perror("scandir");
        exit(EXIT_FAILURE);
    }
    listat=malloc(sizeof(struct stat)*n);

    for(int i=0; i<n;i++){
        char c[strlen(dir_name)+2+strlen(lista[i]->d_name)];
        snprintf(c, strlen(dir_name)+2+strlen(lista[i]->d_name), "%s/%s", dir_name, lista[i]->d_name);
        if(stat(c, &(listat[i]))==-1){
            perror("stat");
            exit(EXIT_FAILURE);
        }       
    }

    int indice_new=0;
    int indice_old=0;
    FILE *log = fopen(log_file, "a");
    while(indice_new<n || indice_old<num_ficheros)
    {
        if(indice_new==n){
            fich_delete(ficheros[indice_old]->d_name, log);
            indice_old++;
        }
        else if(indice_old==num_ficheros){
            fich_creation(lista[indice_new]->d_name, log);
            indice_new++;
        }
        else if (lista[indice_new]->d_ino != ficheros[indice_old]->d_ino)
        {
            if(lista[indice_new]->d_ino > ficheros[indice_old]->d_ino){
            fich_delete(ficheros[indice_old]->d_name, log);
            indice_old++;
            }
            else if(lista[indice_new]->d_ino < ficheros[indice_old]->d_ino){
                fich_creation(lista[indice_new]->d_name, log);
                indice_new++;
            }
        }
        else
        {
            struct stat viejo;
            struct stat nuevo;
            viejo=fichstat[indice_old];
            nuevo=listat[indice_new];
            if (strncmp(ficheros[indice_old]->d_name, lista[indice_new]->d_name, 256) != 0)
            {
                fich_name(lista[indice_new]->d_name, ficheros[indice_old]->d_name, log);
            }
            else if (viejo.st_size != nuevo.st_size)
            {
                fich_size(lista[indice_new]->d_name, viejo.st_size, nuevo.st_size, log);
            }
            else if (viejo.st_mtim.tv_sec != nuevo.st_mtim.tv_sec)
            {
                fich_date(lista[indice_new]->d_name, viejo.st_mtim.tv_sec, nuevo.st_mtim.tv_sec, log);
            }
            indice_new++;
            indice_old++;
        }
    }
    fflush(log);
    fclose(log);


    free(fichstat);

    fichstat=listat;

    for (int i = 0; i < num_ficheros; i++)
    {
        free(ficheros[i]);
    }

    free(ficheros);

    ficheros = lista;

    num_ficheros = n;
}

// Función encargada de limpiar el archivo log
void borrar_log()
{
    FILE *log = fopen(log_file, "w");
    if (log)
    {
        fclose(log);
    }
}

// Función encargada de mostrar la utilización del programa
void print_help()
{
    fprintf(stderr, "Usage: ./watchdir [-n SECONDS] [-l LOG] [DIR]\n"
                "\tSECONDS Refresh rate in [1..60] seconds [default: 1].\n"
                "\tLOG     Log file.\n"
                "\tDIR     Directory name [default: '.'].\n\n");
}

// Manejador de alarmas, recibe 2 tipos de alarmas, SIGALRM sirve para realizar la comparación
// de los ficheros y actualizar el fichero log, y SIGUSR1 sirve para limpiar el fichero log
void handler(int signum)
{
    if (signum == SIGALRM)
    {
        monitordirec();
    }
    else if (signum == SIGUSR1)
    {
        borrar_log();
    }
}

// Función encargada de arrancar el programa
int main(int argc, char **argv)
{
    pid_t pid = 0;
    int c;

    // Se obtienen los parámetros si hay
    while ((c = getopt(argc, argv, "n:l:h")) != -1)
    {
        switch (c)
        {
        case 'n':
            seconds = atoi(optarg);
            break;
        case 'l':
            strncpy(log_file, optarg, sizeof(log_file));
            break;
        case 'h':
            print_help("./watchdir");
            exit(0);
            break;
        case '?':
            print_help("./watchdir");
            exit(EXIT_FAILURE);
            break;    
        default:
            fprintf(stderr, "./watchdir: invalid option -- '%c'\n", c);
            print_help("./watchdir");
            exit(EXIT_FAILURE);
            break;
        }
        
    }

    int agleft = argc-optind;

    if (seconds < 1 || seconds > 60)
    {
        fprintf(stderr, "ERROR: SECONDS must be a value in [1..60].\n");
        print_help();
        exit(EXIT_FAILURE);
    }

    if (agleft == 1)
    {
        strncpy(dir_name, argv[argc - 1], sizeof(dir_name));
    }
    else if(agleft>1){
        fprintf(stderr, "ERROR: './watchdir' does not support more than one directory.\n");
        print_help();
        exit(EXIT_FAILURE);
    }

    // Creación de las señales y se les asigna su manejador
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        fprintf(stderr, "ERROR: sigaction()\n");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("SIGACTION(SIGUSR1)");
        exit(EXIT_FAILURE);
    }

    // Creación del temporizador para que la señal se active en el intervalo indicado
    struct itimerval timer;
    timer.it_value.tv_sec = seconds;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = seconds;
    timer.it_interval.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
    {
        fprintf(stderr, "ERROR: setitimer()\n");
        exit(EXIT_FAILURE);
    }

    num_ficheros = scandir(dir_name, &ficheros, filter, sort);

    if (num_ficheros < 0)
    {
        if(errno==ENOTDIR)
        {
            fprintf(stderr, "ERROR: '%s' is not a directory.\n", dir_name);
        print_help();
        }
        else fprintf(stderr, "ERROR: scandir()\n");
        exit(EXIT_FAILURE);
    }

    fichstat = malloc(sizeof(struct stat)*num_ficheros);

    for(int i=0; i<num_ficheros;i++){
        char c[strlen(dir_name)+2+strlen(ficheros[i]->d_name)];
        snprintf(c, strlen(dir_name)+2+strlen(ficheros[i]->d_name), "%s/%s", dir_name, ficheros[i]->d_name);
        if(stat(c, &(fichstat[i]))==-1){
            fprintf(stderr, "ERROR: stat()\n");
            exit(EXIT_FAILURE);
        }       
    }
    FILE *log = fopen(log_file, "a");
    // Recorre y muestra los nombres de los archivos en el directorio
    for (int i = 0; i < num_ficheros; i++)
    {
        fich_creation(ficheros[i]->d_name, log);
    }
    fflush(log);
    fclose(log);

    

    while (1)
    {
        pause();
    }
}