#ifndef _SLAVE_LIST_PATH_H_
#define _SLAVE_LIST_PATH_H_

/*
    SLAVE-LIST-PATH

    Gets the absolute path of the slave-list.txt file based on the path
    used to execute the particular ethercat application.

*/

void get_app_path(const char *program_path, char *path);
int get_root_dir_index(const char *program_name);
char *get_slave_list_filename(const char *program_name);

#endif // _SLAVE_LIST_PATH_H_
