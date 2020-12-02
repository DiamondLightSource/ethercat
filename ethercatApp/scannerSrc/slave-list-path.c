#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>


void get_app_path(const char *program_path, char *path)
{

    char buf[PATH_MAX];
    if (program_path[0] == '/')
    {
        // Absolute path used
        strcpy(buf, program_path);
    }
    else
    {
        // Relative path used
        if(NULL == getcwd(buf, PATH_MAX))
        {
            perror("Error getting current directory");
            exit(EXIT_FAILURE);
        }
        strcat(buf, "/");
        strcat(buf, program_path);
    }
    if (NULL == realpath(buf, path))
    {
        perror("Error getting real path");
        exit(EXIT_FAILURE);
    }

}


int get_root_dir_index(const char *program_name)
{
    // Search for the binary path
    char binary_dir[] = "bin/linux-x86_64/";

    // Find the binary path in the program path and return the pointer
    char *found = strstr(program_name, binary_dir);

    // Handle the case where it is not found
    if (found == NULL)
    {
        return -1;
    }
    
    // Calculate the difference in the pointers to get the index
    return found - program_name;

}


char *get_slave_list_filename(const char *program_path)
{

    char relative_path[] = "etc/scripts/slave-types.txt";
    char *slave_list_filename = NULL;

    // Get absolute path of application
    char *real_path = calloc(PATH_MAX, sizeof(char));
    get_app_path(program_path, real_path);

    // Get root directory
    int root_dir_index = get_root_dir_index(real_path);
    if (root_dir_index != -1)
    {
        slave_list_filename = calloc(root_dir_index + strlen(relative_path) + 1, sizeof(char));
        strncpy(slave_list_filename, real_path, root_dir_index);
    }

    // Append relative path
    strcat(slave_list_filename, relative_path);

    // Check file
    struct stat fstat;
    int result = stat(slave_list_filename, &fstat);
    if (result)
    {
        printf("Could not find slave list file at %s\n", slave_list_filename);
    }

    // Cleanup
    free(real_path);

    return slave_list_filename;

}
