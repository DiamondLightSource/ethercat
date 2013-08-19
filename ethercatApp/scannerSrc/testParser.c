#include <stdlib.h>
#include <string.h>

#include "classes.h"
#include "parser.h"

int main()
{
    EC_CONFIG * cfg = calloc(1, sizeof(EC_CONFIG));
    char * xml_filename  = "/tmp/expanded1.xml";
    char * text = load_config(xml_filename);
    read_config2(text, strlen(text), cfg);
}
