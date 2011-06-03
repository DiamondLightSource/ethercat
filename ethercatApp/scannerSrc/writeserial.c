#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

char * usage = 
    "usage: %s serial filename\n(serial in decimal)\n\n" 
    "example:\n" 
    "ethercat sii_read -p 0 > flash0\n" 
    "cp flash0 flash0.new\n" 
    "writeserial 123 flash0.new\n" 
    "ethercat sii_write -p 0 flash0.new\n" 
    "# cycle power\n" 
    "ethercat slaves -v\n";

int main(int argc, char ** argv)
{
    if(argc != 3)
    {
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }
    int32_t serial = atoi(argv[1]);
    char * filename = argv[2];
    int offset = 0xE * 2;
    FILE * f = fopen(filename, "r+");
    if(f == NULL)
    {
        perror("open");
        fprintf(stderr, "error: can't open SII dump file %s for editing\n", filename);
        exit(1);
    }
    
    if(fseek(f, offset, SEEK_SET) != 0)
    {
        perror("seek");
        fprintf(stderr, "error: can't seek to serial number offset (%d) in %s\n", offset, filename);
        exit(1);
    }

    if(fwrite(&serial, sizeof(int32_t), 1, f) != 1)
    {
        perror("write");
        fprintf(stderr, "error: can't write serial number to file %s\n", filename);
    }

    fclose(f);
    return 0;
    
}
