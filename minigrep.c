#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define BUFFER_DEFAULT_SIZE 1024
#define BUFFER_LINE_SIZE 4096
#define MAX_LINE_SIZE 4096
#define MB 1048576

// Función que muestra por pantalla un mensaje sobre el uso adecuado del programa
void print_help(char *program_name)
{
    fprintf(stderr, "Uso: %s -r REGEX [-s BUFSIZE] [-v] [-h]\n"
                    "\t-r REGEX Expresión regular.\n"
                    "\t-s BUFSIZE Tamaño de los buffers de lectura y escritura en bytes (por defecto, 1024).\n"
                    "\t-v Muestra las líneas que NO sean reconocidas por la expresión regular.\n\n",
            program_name);
}

// Función que escribe lo que haya en el buffer pasado como parametro
// en el descriptor de fichero pasado como parametro
int stream_write(int fd, char *buffer, ssize_t buffer_size)
{
    ssize_t num_written = 0;
    ssize_t num_left = buffer_size;
    char *buffer_left = buffer;

    // Mientras queden datos por escribir, continua
    while (num_left != 0 && (num_written = write(fd, buffer_left, num_left)) != -1)
    {
        num_left -= num_written;
        buffer_left += num_written;
    }

    return num_written == -1 ? -1 : buffer_size;
}

// Función que devulve cual de los buffers tiene menor tamaño
// O mejor dicho, función que devulve el entero de menor valor
// entre los dos enteros pasados como parametro
int least_size(int buffer1_size, int buffer2_size)
{
    return buffer1_size < buffer2_size ? buffer1_size : buffer2_size;
}

// Función que pasa el contenido de un buffer a otro, desde las posiciones indicadas
// Devuelve un entero que indica la cantidad de caracteres que se han intercambiado
int buffer_push(char *OUTbuffer, char *Inbuffer, int amount_to_write, int OUT_pos, int IN_pos)
{
    int i = 0;
    // Se recorren los buffers a partir de las posiciones indicadas
    // y se cambia el contenido del buffer INbuffer por el contenido del buffer OUTbuffer
    while (i < amount_to_write)
    {
        Inbuffer[IN_pos + i] = OUTbuffer[OUT_pos + i];
        i++;
    }
    return amount_to_write;
}

// Función que cambia el ultimo caracter de una cadena si este es 'a' por el caracter 'b'.
// Devuelve un entero, 0 si ha terminado bien, 1 en caso contrario
int change_end_character(char *buffer, ssize_t buffer_size, char a, char b)
{
    if (buffer[buffer_size - 1] == a)
    {
        buffer[buffer_size - 1] = b;
        return 0;
    }
    return 1;
}

// Función que añade al final de una cadena el caracter 'c' en caso de que el último caracter no lo sea.
// Devuelve un entero, 0 si ha terminado bien, 1 en caso contrario
int add_character(char *buffer, int buffer_size, char c, int buffer_pos)
{
    if (buffer[buffer_pos] != c)
    {
        buffer[buffer_pos + 1] == c;
        return 0;
    }
    return 1;
}

// Función que devuelve la posición de la primera instancia del caracter
// pasado por parametro en la cadena pasada por parametro
int find_next(char *buffer, int buffer_size, int buffer_pos, char c)
{
    if (buffer_size == 1)
    {
        return (buffer[buffer_pos] != c) ? -1 : buffer_pos;
    }

    int end = buffer_size + buffer_pos;
    while (buffer_pos < end && buffer[buffer_pos] != c)
        buffer_pos++;
    return (buffer[buffer_pos] != c) ? -1 : buffer_pos;
}

// Función que se encarga de rellenar el buffer solo con las lineas
// que concuerdan con la expresión regular y devolver el tamaño del buffer reducido
int valid_lines(char *line_buffer, ssize_t buffer_size, regex_t *expression, int r_flag)
{
    int r_value;

    r_value = regexec(expression, line_buffer, 0, NULL, 0);
    if (!r_value)
    {
        if (r_flag)
        {
            return 0;
        }
        return buffer_size;
    }
    else if (r_value == REG_NOMATCH)
    {
        if (r_flag)
        {
            return buffer_size;
        }
        return 0;
    }
    else
    {
        regerror(r_value, expression, line_buffer, buffer_size);
        fprintf(stderr, "Regex match failed: %s\n", line_buffer);
        exit(EXIT_FAILURE);
    }
}

// Función que devuelve la diferencia entre los dos numeros pasados por parametro
int get_space(int buffer_size, int amount)
{
    return buffer_size - amount;
}

// Función que pone en marcha el minigrep
void motion(char *read_buffer, char *line_buffer, char *write_buffer, ssize_t buffer_size, ssize_t line_buffer_size, regex_t *expression, int r_flag)
{
    int read_buffer_filled = 0;
    int line_buffer_filled = 0;
    int free_write_buffer = buffer_size;
    int read_buffer_pos = 0;
    int line_buffer_pos = 0;
    int n_pos;
    int writing = 0;
    int num_written = 0;

    // 1. Mientras se pueda leer por entrada estandar,
    // leer de la entrada estandar y pasar el contenido al buffer de lectura
    while ((read_buffer_filled = read(STDIN_FILENO, read_buffer, buffer_size)) > 0)
    {
        // 2. Mientras queden datos en el buffer de lectura
        // pasar el contenido del buffer de lectura al de linea
        while (read_buffer_filled > 0)
        {
            // n_pos es donde se encuentra el siguiente caracter '\n'
            // dentro de la cadena a partir de la posición pasada por parametro
            n_pos = find_next(read_buffer, read_buffer_filled, read_buffer_pos, '\n');
            if (n_pos == -1)
                n_pos = read_buffer_filled + read_buffer_pos - 1;
            // least_size se usa para aquellos caso en los que al buffer de lectura
            // no le quedan suficientes datos para llenar el buffer de linea
            num_written = buffer_push(read_buffer, line_buffer, least_size(n_pos - read_buffer_pos + 1, get_space(line_buffer_size, line_buffer_filled)), read_buffer_pos, line_buffer_filled);
            read_buffer_filled -= num_written;
            read_buffer_pos += num_written;
            line_buffer_filled += num_written;

            // 3. Comporbar si las lineas en el buffer de linea son validas
            // Se convierten los '\n' en '\0' para poder tratar las lineas con regex
            if (!change_end_character(line_buffer, line_buffer_filled, '\n', '\0'))
            {
                if ((line_buffer_filled = valid_lines(line_buffer, line_buffer_filled, expression, r_flag)) != 0)
                {
                    writing = 1;
                    change_end_character(line_buffer, line_buffer_filled, '\0', '\n');
                }
                else
                {
                    writing = 0;
                }
            }
            else
            {
                if (get_space(line_buffer_size, line_buffer_filled) == 0)
                {
                    fprintf(stderr, "ERROR: Línea demasiado larga\n");
                    exit(EXIT_FAILURE);
                }
                writing = 0;
            }
            if (get_space(line_buffer_size, line_buffer_filled) < 4)
            {
                n_pos = 0;
            }

            // 4. Mientras queden datos en el buffer de linea
            // pasar las lineas validas del buffer de linea al de escritura
            while (writing && line_buffer_filled > 0)
            {
                // least_size se usa para aquellos caso en los que al buffer de linea
                // no le quedan suficientes datos para llenar el buffer de escritura
                num_written = buffer_push(line_buffer, write_buffer, least_size(free_write_buffer, line_buffer_filled), line_buffer_pos, get_space(buffer_size, free_write_buffer));
                line_buffer_filled -= num_written;
                line_buffer_pos += num_written;
                free_write_buffer -= num_written;

                // 5. Escribir el contenido del buffer de escritura en la salida estandar
                // Solo si el buffer de escritura está lleno
                if (free_write_buffer == 0)
                {
                    if (stream_write(STDOUT_FILENO, write_buffer, buffer_size) == -1)
                    {
                        fprintf(stderr, "ERROR: write()\n");
                        exit(EXIT_FAILURE);
                    }
                    free_write_buffer = buffer_size;
                }
            }
            line_buffer_pos = 0;
        }
        read_buffer_pos = 0;
    }
    if (read_buffer_filled == -1)
    {
        fprintf(stderr, "ERROR: read()\n");
        exit(EXIT_FAILURE);
    }
    if (line_buffer_filled != 0)
    {
        if (!add_character(line_buffer, line_buffer_size, '\n', line_buffer_filled - 1))
        {
            line_buffer_filled++;
        }
        if ((line_buffer_filled = valid_lines(line_buffer, line_buffer_filled, expression, r_flag)) != 0)
            change_end_character(line_buffer, line_buffer_filled, '\0', '\n');
    }
    while (line_buffer_filled > 0)
    {
        // least_size se usa para aquellos caso en los que al buffer de linea
        // no le quedan suficientes datos para llenar el buffer de escritura
        num_written = buffer_push(line_buffer, write_buffer, least_size(free_write_buffer, line_buffer_filled), line_buffer_pos, get_space(buffer_size, free_write_buffer));
        line_buffer_filled -= num_written;
        line_buffer_pos += num_written;
        free_write_buffer -= num_written;

        // 5. Escribir el contenido del buffer de escritura en la salida estandar
        // Solo si el buffer de escritura está lleno
        if (free_write_buffer == 0)
        {
            num_written = stream_write(STDOUT_FILENO, write_buffer, buffer_size);
            if (num_written == -1)
            {
                fprintf(stderr, "ERROR: write()\n");
                exit(EXIT_FAILURE);
            }
            free_write_buffer = buffer_size;
        }
    }
    if (free_write_buffer != buffer_size)
    {
        if (stream_write(STDOUT_FILENO, write_buffer, buffer_size - free_write_buffer) == -1)
        {
            fprintf(stderr, "ERROR: write()\n");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv)
{
    char *expression_text = NULL;
    regex_t expression;

    ssize_t buffer_size = BUFFER_DEFAULT_SIZE;
    ssize_t line_buff_size = BUFFER_LINE_SIZE;
    int opt, optint, r_flag = 0;

    char *read_buffer;
    char *write_buffer;
    char *line_buffer;

    // Se extraen los parametros
    while ((opt = getopt(argc, argv, "r:s:vh")) != -1)
    {
        switch (opt)
        {
        case 'r':
            expression_text = optarg;
            break;
        case 's':
            buffer_size = atoi(optarg);
            break;
        case 'v':
            r_flag = 1;
            break;
        case 'h':
            print_help(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Se asigna memoria al buffer de lectura
    if ((read_buffer = (char *)malloc(buffer_size * sizeof(char))) == NULL)
    {
        fprintf(stderr, "ERROR: malloc()\n");
        exit(EXIT_FAILURE);
    }
    // Se asigna memoria al buffer de escritura
    if ((write_buffer = (char *)malloc(buffer_size * sizeof(char))) == NULL)
    {
        fprintf(stderr, "ERROR: malloc()\n");
        exit(EXIT_FAILURE);
    }
    // Se asigna memoria al buffer de linea
    if ((line_buffer = (char *)malloc(line_buff_size * sizeof(char))) == NULL)
    {
        fprintf(stderr, "ERROR: malloc()\n");
        exit(EXIT_FAILURE);
    }

    // Comprobación de que la cadena que representa
    // la expresión regular se ha introducido por parámetro
    if (expression_text == NULL)
    {
        fprintf(stderr, "ERROR: REGEX vacía\n");
        exit(EXIT_FAILURE);
    }
    // Comprobación de que la expresión regex es válida
    if (regcomp(&expression, expression_text, REG_EXTENDED | REG_NEWLINE))
    {
        fprintf(stderr, "ERROR: REGEX mal construida\n");
        exit(EXIT_FAILURE);
    }
    // Comprobación de que el tamaño asignado al buffer es superior a 1
    if (buffer_size < 1 || buffer_size > MB)
    {
        fprintf(stderr, "ERROR: BUFSIZE debe ser mayor que 0 y menor que o igual a 1 MB\n");
        exit(EXIT_FAILURE);
    }

    motion(read_buffer, write_buffer, line_buffer, buffer_size, line_buff_size, &expression, r_flag);

    free(read_buffer);
    free(write_buffer);

    exit(EXIT_SUCCESS);
}