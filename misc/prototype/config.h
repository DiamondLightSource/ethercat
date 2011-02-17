#include <stdint.h>
#include <ecrt.h>

#define CONFIG_FILE "channel-map.txt"

/* maps virtual channel number to offset and size in PDO */
struct channel_map_entry
{
    uint8_t channel;
    uint8_t assigned;    /* set to 0 for fill-in entries*/
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    unsigned int offset;
    unsigned int bit_position;
    uint16_t alias;
};
typedef struct channel_map_entry channel_map_entry;

#define END_CHANNEL 0xFF

int read_configuration(channel_map_entry *mapping, int max_channels);
void show_registration(ec_pdo_entry_reg_t *entry);
