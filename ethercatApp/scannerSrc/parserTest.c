
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <assert.h>
#include <ellLib.h>
#include "classes.h"
#include "parser.h"

int main(int argc, char ** argv)
{
    LIBXML_TEST_VERSION;
    
    char * config_str;
    assert(argc > 1);
    config_str = load_config(argv[1]);
    assert(config_str);
    EC_CONFIG * cfg = calloc(1, sizeof(EC_CONFIG));
    read_config2(config_str, strlen(config_str), cfg);
    
    return 0;
}
