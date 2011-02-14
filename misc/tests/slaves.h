/* Master 0, Slave 2, "EL2004"
 * Vendor ID:       0x00000002
 * Product code:    0x07d43052
 * Revision number: 0x00100000
 */

ec_pdo_entry_info_t slave_2_pdo_entries[] = {
    {0x7000, 0x01, 1}, /* Output */
    {0x7010, 0x01, 1}, /* Output */
    {0x7020, 0x01, 1}, /* Output */
    {0x7030, 0x01, 1}, /* Output */
};

ec_pdo_info_t slave_2_pdos[] = {
    {0x1600, 1, slave_2_pdo_entries + 0}, /* Channel 1 */
    {0x1601, 1, slave_2_pdo_entries + 1}, /* Channel 2 */
    {0x1602, 1, slave_2_pdo_entries + 2}, /* Channel 3 */
    {0x1603, 1, slave_2_pdo_entries + 3}, /* Channel 4 */
};

ec_sync_info_t slave_2_syncs[] = {
    {0, EC_DIR_OUTPUT, 4, slave_2_pdos + 0, EC_WD_ENABLE},
    {0xff}
};

/* Master 0, Slave 10, "EL4102"
 * Vendor ID:       0x00000002
 * Product code:    0x10063052
 * Revision number: 0x03fa0000
 */

ec_pdo_entry_info_t slave_10_pdo_entries[] = {
    {0x3001, 0x01, 16}, /* Output */
    {0x3002, 0x01, 16}, /* Output */
};

ec_pdo_info_t slave_10_pdos[] = {
    {0x1600, 1, slave_10_pdo_entries + 0}, /* RxPDO 01 mapping */
    {0x1601, 1, slave_10_pdo_entries + 1}, /* RxPDO 02 mapping */
};

ec_sync_info_t slave_10_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 2, slave_10_pdos + 0, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {0xff}
};

