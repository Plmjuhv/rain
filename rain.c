////////////////////////////////////////////////////////////////////////
// COMP1521 23T1 --- Assignment 2: `rain', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/23T1/assignments/ass2/index.html>
//
// Written by Scott Tredinnick (z5258051) on __/04/2023
//
// 2021-11-08   v1.1    Team COMP1521 <cs1521 at cse.unsw.edu.au>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include "rain.h"


// ADD ANY extra #defines HERE
#define MAX_PATHNAME_LENGTH 65535
#define MAX_CONTENT_LENGTH (int) pow(2, 48) - 1
#define PERMISSION_LENGTH 10
#define CONTENT_LENGTH 6

// ADD YOUR FUNCTION PROTOTYPES HERE
void print_file_info (FILE *drop_fp, int pathname_length, uint64_t *completed_size);

int get_pathname(FILE *drop_fp, char *pathname, int completed_size);
uint64_t get_content_length(FILE *drop_fp, int pathname_length, uint64_t completed_size);

void check_hash(FILE *drop_fp, uint64_t completed_size, uint64_t file_size);

void set_permissions(FILE *drop_fp, uint64_t completed_size, char *pathname);

void create_drop_directory(int format, char *pathname, FILE *drop_fp);

void create_drop_bytes(int format, char *pathnames, FILE *drop_fp, int check_dir);

void add_prev_dir(char *pathname, FILE *drop_fp, int format, int add_drop);


// Subset 0
// print the files & directories stored in drop_pathname
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed

void list_drop(char *drop_pathname, int long_listing) {
    FILE *drop_fp = fopen(drop_pathname, "r");
    if (drop_fp == NULL) {
        perror(drop_pathname);
        fclose(drop_fp);
        return;
    }

    uint64_t completed_size = 0;

    while(fgetc(drop_fp) != EOF) {
        char pathname[MAX_PATHNAME_LENGTH] = {0};
        int pathname_length = get_pathname(drop_fp, pathname, completed_size);

        if (long_listing) {
            print_file_info(drop_fp, pathname_length, &completed_size);
            printf("  ");
        } else {
            uint64_t content_length = get_content_length(drop_fp, pathname_length,  completed_size);
            completed_size += 21 + content_length + pathname_length;
        }
        
        printf("%s\n", pathname);

        fseek(drop_fp, completed_size, SEEK_SET);
    }

    fclose(drop_fp);
}


// Subset 1
// check the files & directories stored in drop_pathname
//
// prints the files & directories stored in drop_pathname with a message
// either, indicating the hash byte is correct, or
// indicating the hash byte is incorrect, what the incorrect value is and the correct value would be

void check_drop(char *drop_pathname) {
    FILE *drop_fp = fopen(drop_pathname, "r");
    if (drop_fp == NULL) {
        perror(drop_pathname);
        fclose(drop_fp);
        return;
    }

    uint64_t completed_size = 0;
    int magic_num = 0;

    while((magic_num = fgetc(drop_fp)) != EOF) {
        if (magic_num != 'c') {
            fprintf(stderr, "error: incorrect first droplet byte: 0x%2x should be 0x63\n", magic_num);
            return;
        }

        char pathname[MAX_PATHNAME_LENGTH] = {0};        
        int pathname_length = get_pathname(drop_fp, pathname, completed_size);
        printf("%s - ", pathname);
        
        uint64_t content_length = get_content_length(drop_fp, pathname_length,  completed_size);

        uint64_t file_size = 21 + content_length + pathname_length;
        check_hash(drop_fp, completed_size, file_size);

        completed_size += file_size;

        fseek(drop_fp, completed_size, SEEK_SET);
    }

    fclose(drop_fp);
}


// Subset 1 & 3
// extract the files/directories stored in drop_pathname

void extract_drop(char *drop_pathname) {
    FILE *drop_fp = fopen(drop_pathname, "r");
    if (drop_fp == NULL) {
        perror(drop_pathname);
        fclose(drop_fp);
        return;
    }

    uint64_t completed_size = 0;

    while(fgetc(drop_fp) != EOF) {
        char pathname[MAX_PATHNAME_LENGTH] = {0};
        int pathname_length = get_pathname(drop_fp, pathname, completed_size);
        printf("Extracting: %s\n", pathname);

        uint64_t content_length = get_content_length(drop_fp, pathname_length,  completed_size);

        uint64_t file_size = 21 + content_length + pathname_length;
        fseek(drop_fp, completed_size + 20 + pathname_length, SEEK_SET);

        FILE *extract_file = fopen(pathname, "w+");

        for (int i = 0; i < content_length; i++) {
            if (fputc(fgetc(drop_fp), extract_file) == EOF) {
                perror(pathname);
                fclose(drop_fp);
                return;
            }
        }

        set_permissions(drop_fp, completed_size, pathname);

        completed_size += file_size;
        fseek(drop_fp, completed_size, SEEK_SET);
    }

    fclose(drop_fp);
}


// Subset 2 & 3
// create drop_pathname containing the files or directories specified in pathnames
//
// if append is zero drop_pathname should be over-written if it exists
// if append is non-zero droplets should be instead appended to drop_pathname if it exists
//
// format specifies the droplet format to use, it must be one DROPLET_FMT_6,DROPLET_FMT_7 or DROPLET_FMT_8

void create_drop(char *drop_pathname, int append, int format,
                int n_pathnames, char *pathnames[n_pathnames]) {
    FILE *drop_fp;
    if (append) {
        drop_fp = fopen(drop_pathname, "a");
    } else {
        drop_fp = fopen(drop_pathname, "w+");
    }
    if (drop_fp == NULL) {
        perror(drop_pathname);
        fclose(drop_fp);
        return;
    }

    for (int i = 0; i < n_pathnames; i++) {
        int add_drop = 0;
        add_prev_dir(pathnames[i], drop_fp, format, add_drop);
        create_drop_bytes(format, pathnames[i], drop_fp, 1);

        struct stat file_stat;
        if (stat(pathnames[i], &file_stat) != 0) {
            perror(pathnames[i]);
            fclose(drop_fp);
            return;
        }
    }

    fclose(drop_fp);
}


// ADD YOUR EXTRA FUNCTIONS HERE
// Print the permissions, droplet format, and content size of file/directory
void print_file_info (FILE *drop_fp, int pathname_length, uint64_t *completed_size) {
    char permissions;

    fseek(drop_fp, 2 + *completed_size, SEEK_SET);
    for (int i = 0; i < PERMISSION_LENGTH; i++) {
        permissions = fgetc(drop_fp);
        printf("%c", permissions);
    }

    fseek(drop_fp, 1 + *completed_size, SEEK_SET);
    uint8_t format = fgetc(drop_fp);
    printf("  %c", format);


    uint64_t content_length = get_content_length(drop_fp, pathname_length,  *completed_size);
    *completed_size += 21 + content_length + pathname_length;

    printf("  %5lu", content_length);
}

// Gets the pathname of a droplet and returns the length of the pathname
int get_pathname(FILE *drop_fp, char *pathname, int completed_size) {
    fseek(drop_fp, 12 + completed_size, SEEK_SET);
    int pathname_length = (fgetc(drop_fp));
    pathname_length |= (fgetc(drop_fp) << 8);

    for (int i = 0; i < pathname_length; i++) {
        pathname[i] = fgetc(drop_fp);
    }

    return pathname_length;
}

// Get the length of the content stored in a droplet
uint64_t get_content_length(FILE *drop_fp, int pathname_length, uint64_t completed_size) {
    fseek(drop_fp, 14 + pathname_length + completed_size, SEEK_SET);

    uint64_t content_length = 0;
    for (int i = 0; i < CONTENT_LENGTH; i++) {
        uint64_t byte = fgetc(drop_fp);
        content_length |= (byte << (8 * i));
    }

    return content_length;
}

// Check whether the hash of the file is correct and print a message
void check_hash(FILE *drop_fp, uint64_t completed_size, uint64_t file_size) {
    fseek(drop_fp, completed_size, SEEK_SET);

    uint8_t hash = 0;
    for (int i = 0; i < file_size - 1; i++) {
        hash = droplet_hash(hash, fgetc(drop_fp));
    }
    
    uint8_t correct_hash = fgetc(drop_fp);

    if (hash == correct_hash) {
        printf("correct hash\n");
    } else {
        printf("incorrect hash 0x%2x should be 0x%2x\n", hash, correct_hash);
    }
}

// Set the permission for a file 
void set_permissions(FILE *drop_fp, uint64_t completed_size, char *pathname) {
    char perm_char;
    char perm_char_array[3] = {'r', 'w', 'x'};
    int perm_int[3] = {0};
    char permissions[4];

    fseek(drop_fp, 3 + completed_size, SEEK_SET);

    for (int i = 0; i < PERMISSION_LENGTH; i++) {
        perm_char = fgetc(drop_fp);

        for (int j = 0; j < 3; j++) {
            if (perm_char == perm_char_array[j]) {
                perm_int[i / 3] += pow(2, 2 - j);
            }
        }
    }
    
    int index = 0;
    for (int i = 0; i < 3; i++) {
        index += snprintf(&permissions[index], 4 - index, "%d", perm_int[i]);
    }

    char *end;
    mode_t extract_mode = strtol(permissions, &end, 8);

    if (chmod(pathname, extract_mode) != 0) {
        perror(pathname);
        fclose(drop_fp);
        return;
    }
}

// Create droplets from files within directories
// If the file is another directory go inside and create droplets for them too
void create_drop_directory(int format, char *pathname, FILE *drop_fp) {
    DIR *drop_dir = opendir(pathname);
    if (drop_dir == NULL) {
        perror(pathname);
        closedir(drop_dir);
        return;
    }

    struct dirent *dir_file;

    char curr_dir[2] = ".";
    char up_dir[3] = "..";
    while ((dir_file = readdir(drop_dir)) != NULL) {
        if (strcmp(dir_file->d_name, up_dir) == 0 || strcmp(dir_file->d_name, curr_dir) == 0) {
            continue;
        }
        int path_length = strlen(pathname) + strlen(dir_file->d_name) + 2;
        char *sub_path = malloc(path_length);
        snprintf(sub_path, path_length, "%s/%s", pathname, dir_file->d_name);

        create_drop_bytes(format, sub_path, drop_fp, 1);
    }

    closedir(drop_dir);
} 

// Create the bytes for a droplet and place them in the drop
void create_drop_bytes(int format, char *pathname, FILE *drop_fp, int check_dir) {
    printf("Adding: %s\n", pathname);
    int magic = 'c';
    int hash = 0;

    fputc(magic, drop_fp);
    hash = droplet_hash(hash, magic);

    fputc(format, drop_fp);
    hash = droplet_hash(hash, format);

    struct stat file_stat;
    if (stat(pathname, &file_stat) != 0) {
        perror(pathname);
        fclose(drop_fp);
        return;
    }

    uint64_t mask = 0x100;
    char perm_char_array[3] = {'r', 'w', 'x'};

    if (S_ISDIR(file_stat.st_mode)) {
        fputc('d', drop_fp);
        hash = droplet_hash(hash, 'd');
    } else {
        fputc('-', drop_fp);
        hash = droplet_hash(hash, '-');
    }

    for (int j = 0; j < 9; j++) {
        if (((mask & file_stat.st_mode) >> (8 - j)) == 1) {
            fputc(perm_char_array[j % 3], drop_fp);
            hash = droplet_hash(hash, perm_char_array[j % 3]);
        } else {
            fputc('-', drop_fp);
            hash = droplet_hash(hash, '-');
        }

        mask >>= 1;
    }

    mask = 0xFF;
    int file_name_length = strlen(pathname);
    for (int j = 0; j < 2; j++) {
        int length_byte = (file_name_length & mask) >> (j * 8);
        fputc(length_byte, drop_fp);
        hash = droplet_hash(hash, length_byte);

        mask <<= 8;
    }

    for (int j = 0; j < file_name_length; j++) {
        fputc(pathname[j], drop_fp);
        hash = droplet_hash(hash, pathname[j]);
    }
    
    uint64_t content_length = 0;
    FILE *drop_file = fopen(pathname, "r");
    while (fgetc(drop_file) != EOF) {
        content_length++;
    }

    mask = 0xFF;
    for (int j = 0; j < 6; j++) {
        int length_byte = (content_length & mask) >> (j * 8);
        fputc(length_byte, drop_fp);
        hash = droplet_hash(hash, length_byte);

        mask <<= 8;
    }

    fseek(drop_file, 0, SEEK_SET);
    for (int j = 0; j < content_length; j++) {
        int content_byte = fgetc(drop_file);
        fputc(content_byte, drop_fp);
        hash = droplet_hash(hash, content_byte);
    }

    fputc(hash, drop_fp);

    if(S_ISDIR(file_stat.st_mode) && check_dir) {
        create_drop_directory(format, pathname, drop_fp);
    }
}

// Add previous directories that were stated in the argument
void add_prev_dir(char *pathname, FILE *drop_fp, int format, int add_drop) {
    int found_prev_dir = 0;
    for (int j = strlen(pathname); j >= 0 && found_prev_dir == 0; j--)  {
        if (pathname[j] == '/') {
            char *prev_dir = strdup(pathname);
            prev_dir[j] = '\0';

            add_prev_dir(prev_dir, drop_fp, format, 1);
            found_prev_dir = 1;
            free(prev_dir);
        }
    }

    if (add_drop == 1) {
        create_drop_bytes(format, pathname, drop_fp, 0);
    }
}