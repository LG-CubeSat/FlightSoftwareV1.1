#include "ccsds/compression.h"

/*
only the config check from compression.c. profile.c needs it, but the rest of
compression.c needs libaec, which the radio framing doesn't use.
*/

ccsds_status_t ccsds_121_validate_config(const ccsds_121_config_t *config)
{
    bool legal_block_size;

    if (config == NULL) {
        return CCSDS_ERROR_NULL;
    }
    legal_block_size = config->block_size == 8U ||
                       config->block_size == 16U ||
                       config->block_size == 32U ||
                       config->block_size == 64U;
    if (config->bits_per_sample < 1U || config->bits_per_sample > 32U ||
        !legal_block_size ||
        config->reference_sample_interval < 1U ||
        config->reference_sample_interval > 4096U ||
        (config->restricted_code_options && config->bits_per_sample > 4U)) {
        return CCSDS_ERROR_RANGE;
    }
    return CCSDS_OK;
}
