#ifdef __cplusplus
extern "C" {
#endif
char * regenerate_chain(EC_CONFIG * cfg);
char * serialize_config(EC_CONFIG * cfg);
char * load_config(char * filename);
int read_config(char * config, int sz, EC_CONFIG * cfg);
int parseEntriesFromBuffer(char * text, int size, EC_CONFIG * cfg);
#ifdef __cplusplus
}
#endif

