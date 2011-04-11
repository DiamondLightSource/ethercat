char * serialize_config(EC_CONFIG * cfg);
int read_config(char * config, char * chain, EC_CONFIG * cfg);
char * load_config(char * filename);
int read_config2(char * config, int sz, EC_CONFIG * cfg);
int parseEntriesFromBuffer(char * text, int size, EC_CONFIG * cfg);

