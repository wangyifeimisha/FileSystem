#include <cstring>
#include <iostream>

#include "FS.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef ECHO
#define ECHO 0
#endif

#define MAX_CMD_ARGS 5

void simple_test() {
    FS_init();
    create("abc");
    int fh = open("abc");
    write(fh, "123456", 6);
    seek(fh, 0);
    char buff[6];
    read(fh, buff, 6);
    create("def");
    close(fh);
    FS_close();
}

/*
●cr <name>
 create a new file with the name <name>
 Output: <name> created
●de <name>
 destroy the named file <name>
 Output: <name> destroyed
 
●op <name>
 open the named file <name> for reading and writing; display an index value
 Output: <name> opened <index>
 
●cl <index>
 close the specified file <index>
 Output: <index> closed
 
●rd <index> <mem> <count>
 copy <count> bytes from open file <index> (starting from current position) to
memory M (starting at location M[<mem>]) oOutput: <n> bytes read from file
<index> where n is the number of characters actually read (less or equal
<count>)  

●wr <index> <mem> <count>
 copy <count> bytes from memory M (starting
at location M[<mem>]) to open file <index> (starting from current position)
 Output: <n> bytes written to file <index>
where n is the number of characters actually written (less or equal <count>)
 
●sk <index> <pos>
 seek: set the current position of the specified file <index> to <pos>
 Output: position is <pos>
 
●dr
 directory: list the names and lengths of all files
 Output: <file0> <len1> <file1> <len2> ... <fileN> <lenN>
 
●in
 initialize the system to the original starting configuration
 Output: system initialized
●rm <mem> <count>
 copy <count> bytes from memory M staring with position <mem> to output device
(terminal or file) oOutput: <xx...x> where each x is a character

●wm <mem> <str>
 copy string <str> into memory M starting with position <mem> from input device
(terminal or file) oOutput: <n> bytes written to M where n is the length of
<str>

●If any command fails, output: error
*/

/* Tokenize the cmd into words, return number of words. */
int tokenize(char* line, char** words);

void solve(FILE* in, FILE* out) {
    // FS_init();
    int init = 0;

    int fh;
    char M[BUFSIZ];
    char line[BUFSIZ], *cmd, *args[MAX_CMD_ARGS];
    memset(M, 0, sizeof(M));
    while (fgets(line, BUFSIZ, in)) {
        memset(args, 0, sizeof(args));
        int words = tokenize(line, args);
        if (words == 0) continue;

#if (ECHO)
        fprintf(out, "CMD:");
        for (int i = 0; i < words; ++i) {
            fprintf(out, " |%s|", args[i]);
        }
        fprintf(out, "\n");
#endif

        cmd = args[0];
        if (strcmp(cmd, "cr") == 0) {
            // create a new file with the name <name>
            // Output: <name> created
            if (create(args[1]) == 0) {
                fprintf(out, "%s created\n", args[1]);
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "de") == 0) {
            // destroy the named file <name>
            // Output: <name> destroyed
            if (destroy(args[1]) == 0) {
                fprintf(out, "%s destroyed\n", args[1]);
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "op") == 0) {
            // open the named file <name> for reading and writing;
            // display an index value
            // Output: <name> opened <index>
            if ((fh = open(args[1])) > 0) {
                fprintf(out, "%s opened %d\n", args[1], fh);
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "cl") == 0) {
            // close the specified file <index>
            // Output: <index> closed
            int index = atoi(args[1]);
            if (index > 0 && close(index) == 0) {
                fprintf(out, "%d closed\n", index);
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "rd") == 0) {
            // copy <count> bytes from open file <index> (starting from current
            // position) to memory M (starting at location M[<mem>])
            // Output: <n> bytes read from file <index>
            // where n is the number of characters actually read (less or equal
            // <count>)
            int index = atoi(args[1]);
            int mem = atoi(args[2]);
            int count = atoi(args[3]);
            int n;
            if (index > 0 && mem >= 0 && count >= 0 &&
                (n = read(index, M + mem, count)) >= 0) {
                fprintf(out, "%d bytes read from %d\n", n, index);
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "wr") == 0) {
            // copy <count> bytes from memory M (starting at location M[<mem>])
            // to open file <index> (starting from current position)
            // Output: <n> bytes written to file <index>
            // where n is the number of characters actually written (less or
            // equal <count>)
            int index = atoi(args[1]);
            int mem = atoi(args[2]);
            int count = atoi(args[3]);
            int n;
            if (index > 0 && mem >= 0 && count >= 0 &&
                (n = write(index, M + mem, count)) >= 0) {
                fprintf(out, "%d bytes written to %d\n", n, index);
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "sk") == 0) {
            // seek: set the current position of the specified file <index> to
            // <pos>
            // Output: position is <pos>
            int index = atoi(args[1]);
            int pos = atoi(args[2]);
            if (index > 0 && pos >= 0 && seek(index, pos) == 0) {
                fprintf(out, "position is %d\n", pos);
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "dr") == 0) {
            // directory: list the names and lengths of all files
            // Output: <file0> <len1> <file1> <len2> ... <fileN> <lenN>
            if (directory() >= 0) {
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "in") == 0) {
            // initialize the system to the original starting configuration
            // Output: system initialized

            //? necessary?
            if (++init > 1) {
                fprintf(out, "\n");
            }

            if (FS_init() == 0) {
                fprintf(out, "system initialized\n");
            } else {
                fprintf(out, "error\n");
            }
        } else if (strcmp(cmd, "rm") == 0) {
            // copy <count> bytes from memory M staring with position <mem> to
            // output device (terminal or file)
            // Output: <xx...x>
            // where each x is a character

            int mem = atoi(args[1]);
            int count = atoi(args[2]);
            fprintf(out, "%-*s\n", count, M + mem);
        } else if (strcmp(cmd, "wm") == 0) {
            // copy string <str> into memory M starting with position <mem> from
            // input device (terminal or file)
            // Output: <n> bytes written to M
            // where n is the length of <str>

            int mem = atoi(args[1]);
            unsigned int n = strlen(args[2]);
            memcpy(M + mem, args[2], n);
            fprintf(out, "%u bytes written to M\n", n);
        } else {
#if (DEBUG)
            printf("Unkown cmd\n");
#endif
            fprintf(out, "error\n");
        }
    }

    FS_close();
}

int main(/* int argc, char** argv */) {
    solve(stdin, stdout);

    return 0;
}

int tokenize(char* line, char** words) {
    int n_cmd_args;

    /* Tokenize the cmdline */
    words[0] = strtok(line, " \t\r\n");
    if (words[0] == NULL || strlen(words[0]) == 0) return 0;

    for (n_cmd_args = 1; (n_cmd_args < MAX_CMD_ARGS) &&
                         (words[n_cmd_args] = strtok(NULL, " \t\r\n"));
         n_cmd_args++)
        ;
    return n_cmd_args;
}
