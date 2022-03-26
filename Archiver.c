#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>

void writefile(char* filename, int out, int depth, bool isDir, long size);
void scan_dir(char *dir, int depth, int out);
void unzip(char *infile, char *dir_name);
void failwrite(int n);

int main(int argc, char* argv[]) {
    char *operation = argv[1];
    if (strcmp(operation, "archive") == 0) {     // prog_name archive path_to_dir out_file_name
        char *topdir = ".", *ofilename;
        topdir = argv[2];
        ofilename = argv[3];
        int out = open(ofilename, O_CREAT | O_WRONLY, S_IRWXU | S_IRWXO);
        if (out == -1) {
            printf("Failed to open output file!\n");
            return 0;
        }
        else {
            printf("Archived directory: %s\n", topdir);
            scan_dir(topdir, 0, out);
            printf("Archiving completed. File name: %s\n", ofilename);
        }
        close(out);
    }
    else if (strcmp(operation, "unzip") == 0) {     // prog_name unzip path_to_file path_to_dir
        char *ifile, *dir_name;
        ifile = argv[2];
        dir_name = argv[3];
        unzip(ifile, dir_name);
    }
    else {
        printf("Invalid input!\n");
        printf("Correct input:\n");
        printf("prog_name archive path_to_dir out_file_name\nor\n");
        printf("prog_name unzip path_to_file path_to_dir\n");
    }
    return 0;
}

void scan_dir(char *dir, int depth, int out) {
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    if((dp=opendir(dir)) == NULL) {
        printf("Can't open directory: %s\n", dir);
        return;
    }
    chdir(dir);
    while((entry=readdir(dp)) != NULL) {
        stat(entry->d_name, &statbuf);
        if(S_ISDIR(statbuf.st_mode)){           // если dir - директория
            /*находит каталог, но игнорирует . и ..*/
            if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
                continue;
            writefile(entry->d_name, out, depth, true, statbuf.st_size);

            scan_dir(entry->d_name,depth + 1, out);
        } 
        else {                                  // если dir - файл
            writefile(entry->d_name, out, depth, false, statbuf.st_size);
        }
    }
    chdir("..");
    closedir(dp);
}
void writefile(char* filename, int out, int depth, bool isDir, long size) {
    printf("\n_____________________________________\n");
    printf("Writing a file into archive..\n");
    int nread, nwrite;
    int in = open(filename, O_RDONLY);
    if(in == -1){
        printf("Error opening file!\n");
        return;
    }
    char type = 'f';  // file or directory
    if(isDir) {
        type = 'd';
    }
    nwrite = write(out, &type, 1);
    failwrite(nwrite);
    printf("Type = %c\n", type);

    nwrite = write(out, &depth, sizeof(depth)); // hierarchy
    failwrite(nwrite);
    printf("Depth = %d\n", depth);
    
    int namesize = strlen(filename);
    nwrite = write(out, &namesize, sizeof(namesize));  // name length
    failwrite(nwrite);
    nwrite = write(out, filename, namesize + 1);           // name
    failwrite(nwrite);
    printf("Size of name = %d; filename = %s\n", namesize, filename);
    nwrite = write(out, &size, sizeof(size));          // size
    failwrite(nwrite);

    printf("Size of file = %ld\n", size);
    if(!isDir) {                              // file contents
        long sum = 0;
        char buf[1024];
        while((nread = read(in, buf, sizeof(buf))) > 0) {
            nwrite = write(out, buf, nread);
            failwrite(nwrite);
            sum += nread;
        }
        printf("Has written: %ld\n", sum);
    }
    printf("\n_____________________________________\n");
    close(in);
}
void unzip(char *infile, char *dir_name) {
    printf("Unpacking started..\n");
    char *topdir, type;
    int in = open(infile, O_RDONLY), n, nread, location = 0;
    if (in == -1) {
        printf("Error opening input file!\n");
        return;
    }
    topdir = dir_name;
    n = mkdir(topdir, S_IRWXU | S_IRWXG | S_IRWXO);
    if (n == -1){
        printf("Failed to create a directory!\n");
        return;
    }
    chdir(topdir);

    while((nread = read(in, &type, sizeof(char))) > 0) {
        int depth, namelength, len = strlen(dir_name);
        long size;
        nread = read(in, &depth, sizeof(int));
        if (nread == -1) {
            printf("Failed to read depth!\n");
            return;
        }
        printf("\n_____________________________________\n");
        printf("Type = %c; depth = %d\n", type, depth);
        while(location > depth) {
            chdir("..");
            location--;
        }

        nread = read(in, &namelength, sizeof(int));
        if (nread == -1) {
            printf("Failed to read name lengh!\n");
            return;
        }
        char* name = (char*)malloc(namelength + 1);

        n = read(in, name, namelength + 1);
        if (n == -1) {
            printf("Failed to read name!\n");
            return;
        }
        printf("Name length = %d; name = %s\n", namelength, name);
        nread = read(in, &size, sizeof(long));
        if (nread == -1) {
            printf("Failed to read size of file/directory!\n");
            return;
        }
        printf("Size of file = %ld", size);
        printf("\n_____________________________________\n");

        if (type == 'd') {
            n = mkdir(name, S_IRWXU | S_IRWXO);
            if (n == -1) {
                //printf("%s\n", path);
                printf("Failed to create a directory!\n");
                return;
            }
            chdir(name);
            location++;
        }
        else {
            int out = open(name, O_CREAT | O_WRONLY, S_IRWXU | S_IRWXO);
            if (out == -1) {
                printf("Failed to open output file!\n");
                return;
            }
            char *buf = (char*)malloc(size);
            nread = read(in, buf, size);
            if (nread == -1) {
                printf("Failed to read file contents!\n");
                return;
            }
            n = write(out, buf, size);
            failwrite(n);
            free(buf);
            close(out);
        }
        free(name);
    }
    close(in);
    printf("Unpacking completed.\n");
}
void failwrite(int n) {
    if(n == -1) {
        printf("Failed to write data to output file!\n");
    }
    return;
}