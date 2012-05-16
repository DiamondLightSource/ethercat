
void simulation_fill(st_signal * signal);
void copy_sim_data(st_signal * signal, 
                   EC_PDO_ENTRY_MAPPING * pdo_entry_mapping, uint8_t * pd);
void copy_sim_data2(st_signal * signal, EC_PDO_ENTRY_MAPPING * pdo_entry_mapping, 
                    uint8_t * pd, int index);
