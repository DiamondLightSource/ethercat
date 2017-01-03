#ifdef __cplusplus
extern "C" {
#endif
char * regenerate_chain(EC_CONFIG * cfg);
char * serialize_config(EC_CONFIG * cfg);
char * load_config(char * filename);
parsing_result_type_t read_config(char * config, int sz, EC_CONFIG * cfg);
parsing_result_type_t parseEntriesFromBuffer(char * text, int size, EC_CONFIG * cfg);
#ifdef __cplusplus
}
#endif

