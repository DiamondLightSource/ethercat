#ifdef __cplusplus
extern "C" {
#endif
void test_ioc_client(char * path, int max_message);
int32_t cast_int32(EC_PDO_ENTRY_MAPPING * mapping, char * buffer, int index);
int32_t sdocast_int32(EC_SDO_ENTRY *sdoentry, SDO_READ_MESSAGE *msg);
double cast_double(EC_PDO_ENTRY_MAPPING * mapping, char * buffer, int index);
int unpack_int(char * buffer, int * ofs);
#ifdef __cplusplus
}
#endif
