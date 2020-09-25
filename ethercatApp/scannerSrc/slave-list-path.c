#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>


void get_app_path(const char *argv0, char *path) {

    char buf[PATH_MAX];
    if (argv0[0] == '/') {
        // Absolute path used
        strcpy(buf, argv0);
    }
    else {
        // Relative path used
        if(NULL == getcwd(buf, PATH_MAX)) {
            perror("Error getting current directory");
        }
        strcat(buf, "/");
        strcat(buf, argv0);
    }
    if (NULL == realpath(buf, path)) {
        perror("Error getting real path");
    }

}


int get_root_dir_index(const char *program_name) {
    // Search for the binary path
    char binary_dir[] = "bin/linux-x86_64/";
    int binary_dir_path_size = sizeof(binary_dir)/sizeof(binary_dir[0]) - 1;
    // Store indices
    int index = 0, binary_dir_index = 0;
    int matched_index = -1;
    const char *c, *c_matcher;
    // Starting character
    for (c = program_name; *c != '\0'; c++) {
        // Check following characters match
        for (c_matcher = c; *c_matcher != '\0'; c_matcher++) {
            if (*c_matcher == binary_dir[binary_dir_index]) {
                binary_dir_index++;
                // Check if we found all of them
                if (binary_dir_index == binary_dir_path_size) {
                    matched_index = index;
                }
            }
            else {
                break;
            }
        }
        index++;
    }

    return matched_index;

}


char *get_slave_list_filename(const char *program_path) {

    char relative_path[] = "etc/scripts/slave-types.txt";
    int relative_path_size = sizeof(relative_path)/sizeof(char);
    char *slave_list_filename = NULL;

    // Get absolute path of application
    char *real_path = calloc(sizeof(char), PATH_MAX);
    get_app_path(program_path, real_path);

    // Get root directory
    int root_dir_index = get_root_dir_index(real_path);
    if (root_dir_index != -1) {
        slave_list_filename = calloc(root_dir_index + relative_path_size, sizeof(char));
        strncpy(slave_list_filename, real_path, root_dir_index);
    }

    // Append relative path
    strcat(slave_list_filename, relative_path);

    // Check file
    struct stat fstat;
    int result = stat(slave_list_filename, &fstat);
    if (result) {
        printf("Could not find slave list file at %s\n", slave_list_filename);
    }

    // Cleanup
    free(real_path);

    return slave_list_filename;

}
