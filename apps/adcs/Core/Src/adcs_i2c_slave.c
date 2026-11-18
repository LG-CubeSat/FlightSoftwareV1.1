#include "adcs_i2c_slave.h"

#include <string.h>

typedef enum
{
    ADCS_RX_PHASE_IDLE = 0,
    ADCS_RX_PHASE_COMMAND,
    ADCS_RX_PHASE_WRITE_HEADER,
    ADCS_RX_PHASE_WRITE_PAYLOAD,
    ADCS_RX_PHASE_COMPLETE,
    ADCS_RX_PHASE_REJECTED
} adcs_rx_phase_t;

typedef enum
{
    ADCS_TX_KIND_NONE = 0,
    ADCS_TX_KIND_STATUS,
    ADCS_TX_KIND_FRAME
} adcs_tx_kind_t;

static I2C_HandleTypeDef *s_i2c = NULL;

static uint8_t s_command_byte = 0U;
static volatile uint8_t s_selected_command = 0U;
static volatile adcs_rx_phase_t s_rx_phase = ADCS_RX_PHASE_IDLE;
static volatile adcs_tx_kind_t s_tx_kind = ADCS_TX_KIND_NONE;

/* Two receive slots keep a completed frame stable until main consumes it. */
static uint8_t s_rx_frames[2][ADCS_I2C_MAX_FRAME_WIRE_SIZE];
static uint16_t s_rx_active_length = 0U;
static uint8_t s_rx_active_index = 0U;
static volatile uint8_t s_rx_ready_index = 0U;
static volatile uint16_t s_rx_ready_length = 0U;
static volatile bool s_rx_ready = false;

/* A queued transmit frame is immutable until the master reads all of it. */
static uint8_t s_tx_frames[2][ADCS_I2C_MAX_FRAME_WIRE_SIZE];
static volatile uint8_t s_tx_ready_index = 0U;
static volatile uint16_t s_tx_ready_length = 0U;
static volatile bool s_tx_ready = false;
static uint8_t s_tx_in_progress_index = 0U;

static uint8_t s_status_response[ADCS_I2C_STATUS_WIRE_SIZE];
static volatile uint32_t s_events = 0U;
static volatile bool s_recovery_requested = false;
static volatile adcs_i2c_stats_t s_stats;

static uint32_t adcs_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void adcs_leave_critical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static void adcs_set_event(uint32_t event)
{
    uint32_t primask = adcs_enter_critical();
    s_events |= event;
    adcs_leave_critical(primask);
}

static bool adcs_wire_frame_valid(const uint8_t *wire, size_t wire_length)
{
    uint16_t payload_length;

    if ((wire == NULL) ||
        (wire_length < ADCS_I2C_FRAME_HEADER_SIZE) ||
        (wire_length > ADCS_I2C_MAX_FRAME_WIRE_SIZE))
    {
        return false;
    }

    payload_length = (uint16_t)(((uint16_t)wire[2] << 8U) | wire[3]);
    return (payload_length <= ADCS_I2C_MAX_FRAME_PAYLOAD) &&
        (wire_length == (size_t)ADCS_I2C_FRAME_HEADER_SIZE + payload_length);
}

static void adcs_note_callback_error(void)
{
    s_stats.callback_error_count++;
    adcs_set_event(ADCS_I2C_EVENT_ERROR);
    s_recovery_requested = true;
}

static void adcs_note_malformed_receive(void)
{
    if ((s_rx_phase == ADCS_RX_PHASE_COMMAND) ||
        (s_rx_phase == ADCS_RX_PHASE_WRITE_HEADER) ||
        (s_rx_phase == ADCS_RX_PHASE_WRITE_PAYLOAD))
    {
        s_stats.malformed_frame_count++;
        adcs_set_event(ADCS_I2C_EVENT_MALFORMED_FRAME);
        s_rx_phase = ADCS_RX_PHASE_REJECTED;
    }
}

static void adcs_publish_received_frame(void)
{
    if (s_rx_ready)
    {
        s_stats.receive_overrun_count++;
        adcs_set_event(ADCS_I2C_EVENT_RX_OVERRUN);
        return;
    }

    s_rx_ready_index = s_rx_active_index;
    s_rx_ready_length = s_rx_active_length;
    s_rx_ready = true;
    s_rx_active_index ^= 1U;
    s_stats.receive_complete_count++;
    adcs_set_event(ADCS_I2C_EVENT_RX_FRAME_READY | ADCS_I2C_EVENT_ACTIVITY);
}

static void adcs_prepare_status_response(void)
{
    uint16_t frame_length = s_tx_ready ? s_tx_ready_length : 0U;

    s_status_response[0] = ADCS_I2C_PROTOCOL_MAGIC;
    s_status_response[1] = ADCS_I2C_PROTOCOL_VERSION;
    s_status_response[2] = s_tx_ready ? ADCS_I2C_STATUS_DATA_READY : 0U;
    s_status_response[3] = (uint8_t)(frame_length >> 8U);
    s_status_response[4] = (uint8_t)(frame_length & 0xFFU);
}

HAL_StatusTypeDef adcs_i2c_slave_init(I2C_HandleTypeDef *i2c)
{
    HAL_StatusTypeDef status;

    if (i2c == NULL)
    {
        return HAL_ERROR;
    }

    memset((void *)&s_stats, 0, sizeof(s_stats));
    s_events = 0U;
    s_recovery_requested = false;
    s_selected_command = 0U;
    s_rx_phase = ADCS_RX_PHASE_IDLE;
    s_tx_kind = ADCS_TX_KIND_NONE;
    s_rx_ready = false;
    s_tx_ready = false;
    s_rx_active_index = 0U;
    s_i2c = i2c;

    status = HAL_I2C_EnableListen_IT(s_i2c);
    if (status != HAL_OK)
    {
        s_i2c = NULL;
    }

    return status;
}

void adcs_i2c_slave_process(void)
{
    HAL_I2C_StateTypeDef state;
    uint32_t primask;

    if (s_i2c == NULL)
    {
        return;
    }

    /* Claim this request so an interrupt can safely post a newer one. */
    primask = adcs_enter_critical();
    if (!s_recovery_requested)
    {
        adcs_leave_critical(primask);
        return;
    }
    s_recovery_requested = false;
    adcs_leave_critical(primask);

    state = HAL_I2C_GetState(s_i2c);
    if (((uint32_t)state & (uint32_t)HAL_I2C_STATE_LISTEN) != 0U)
    {
        return;
    }
    else if (state == HAL_I2C_STATE_READY)
    {
        if (HAL_I2C_EnableListen_IT(s_i2c) == HAL_OK)
        {
            s_stats.recovery_count++;
            adcs_set_event(ADCS_I2C_EVENT_RECOVERED);
            return;
        }
    }

    primask = adcs_enter_critical();
    s_recovery_requested = true;
    adcs_leave_critical(primask);
}

uint32_t adcs_i2c_slave_take_events(void)
{
    uint32_t primask = adcs_enter_critical();
    uint32_t events = s_events;
    s_events = 0U;
    adcs_leave_critical(primask);
    return events;
}

void adcs_i2c_slave_get_stats(adcs_i2c_stats_t *stats_out)
{
    uint32_t primask;

    if (stats_out == NULL)
    {
        return;
    }

    primask = adcs_enter_critical();
    stats_out->address_match_count = s_stats.address_match_count;
    stats_out->receive_start_count = s_stats.receive_start_count;
    stats_out->receive_complete_count = s_stats.receive_complete_count;
    stats_out->transmit_start_count = s_stats.transmit_start_count;
    stats_out->transmit_complete_count = s_stats.transmit_complete_count;
    stats_out->listen_complete_count = s_stats.listen_complete_count;
    stats_out->error_count = s_stats.error_count;
    stats_out->callback_error_count = s_stats.callback_error_count;
    stats_out->invalid_command_count = s_stats.invalid_command_count;
    stats_out->malformed_frame_count = s_stats.malformed_frame_count;
    stats_out->recovery_count = s_stats.recovery_count;
    stats_out->receive_overrun_count = s_stats.receive_overrun_count;
    stats_out->last_error = s_stats.last_error;
    stats_out->last_address_match = s_stats.last_address_match;
    stats_out->last_direction = s_stats.last_direction;
    stats_out->last_command = s_stats.last_command;
    adcs_leave_critical(primask);
}

bool adcs_i2c_slave_take_received_frame(adcs_i2c_frame_t *frame_out)
{
    uint32_t primask;

    if (frame_out == NULL)
    {
        return false;
    }

    primask = adcs_enter_critical();
    if (!s_rx_ready)
    {
        adcs_leave_critical(primask);
        return false;
    }

    frame_out->length = s_rx_ready_length;
    memcpy(frame_out->bytes, s_rx_frames[s_rx_ready_index], s_rx_ready_length);
    s_rx_ready = false;
    adcs_leave_critical(primask);
    return true;
}

HAL_StatusTypeDef adcs_i2c_slave_queue_transmit_frame(
    const uint8_t *wire_frame,
    size_t wire_length
)
{
    uint32_t primask;
    uint8_t next_index;

    if (!adcs_wire_frame_valid(wire_frame, wire_length))
    {
        return HAL_ERROR;
    }

    primask = adcs_enter_critical();
    if (s_tx_ready)
    {
        adcs_leave_critical(primask);
        return HAL_BUSY;
    }

    next_index = s_tx_ready_index ^ 1U;
    memcpy(s_tx_frames[next_index], wire_frame, wire_length);
    s_tx_ready_length = (uint16_t)wire_length;
    s_tx_ready_index = next_index;
    s_tx_ready = true;
    adcs_leave_critical(primask);
    return HAL_OK;
}

void HAL_I2C_AddrCallback(
    I2C_HandleTypeDef *i2c,
    uint8_t transfer_direction,
    uint16_t address_match_code
)
{
    HAL_StatusTypeDef status = HAL_ERROR;
    uint16_t expected_address;

    if ((s_i2c == NULL) || (i2c != s_i2c))
    {
        return;
    }

    s_stats.address_match_count++;
    s_stats.last_address_match = address_match_code;
    s_stats.last_direction = transfer_direction;
    adcs_set_event(ADCS_I2C_EVENT_ACTIVITY);

    expected_address = (uint16_t)(i2c->Init.OwnAddress1 & 0x03FEU);
    if (address_match_code != expected_address)
    {
        s_stats.invalid_command_count++;
        adcs_set_event(ADCS_I2C_EVENT_INVALID_COMMAND);
        __HAL_I2C_GENERATE_NACK(i2c);
        __HAL_I2C_CLEAR_FLAG(i2c, I2C_FLAG_ADDR);
        return;
    }

    if (transfer_direction == I2C_DIRECTION_TRANSMIT)
    {
        s_selected_command = 0U;
        s_command_byte = 0U;
        s_rx_phase = ADCS_RX_PHASE_COMMAND;
        s_stats.receive_start_count++;
        status = HAL_I2C_Slave_Seq_Receive_IT(
            i2c,
            &s_command_byte,
            1U,
            I2C_FIRST_FRAME
        );
    }
    else if (transfer_direction == I2C_DIRECTION_RECEIVE)
    {
        if (s_selected_command == ADCS_I2C_CMD_GET_STATUS)
        {
            adcs_prepare_status_response();
            s_tx_kind = ADCS_TX_KIND_STATUS;
            status = HAL_I2C_Slave_Seq_Transmit_IT(
                i2c,
                s_status_response,
                ADCS_I2C_STATUS_WIRE_SIZE,
                I2C_FIRST_AND_LAST_FRAME
            );
        }
        else if ((s_selected_command == ADCS_I2C_CMD_READ_FRAME) && s_tx_ready)
        {
            s_tx_in_progress_index = s_tx_ready_index;
            s_tx_kind = ADCS_TX_KIND_FRAME;
            status = HAL_I2C_Slave_Seq_Transmit_IT(
                i2c,
                s_tx_frames[s_tx_in_progress_index],
                s_tx_ready_length,
                I2C_FIRST_AND_LAST_FRAME
            );
        }
        else
        {
            /* Return a valid empty status instead of leaving the bus stretched. */
            s_stats.invalid_command_count++;
            adcs_set_event(ADCS_I2C_EVENT_INVALID_COMMAND);
            adcs_prepare_status_response();
            s_tx_kind = ADCS_TX_KIND_STATUS;
            status = HAL_I2C_Slave_Seq_Transmit_IT(
                i2c,
                s_status_response,
                ADCS_I2C_STATUS_WIRE_SIZE,
                I2C_FIRST_AND_LAST_FRAME
            );
        }

        if (status == HAL_OK)
        {
            s_stats.transmit_start_count++;
        }
    }
    else
    {
        s_stats.invalid_command_count++;
        adcs_set_event(ADCS_I2C_EVENT_INVALID_COMMAND);
    }

    if (status != HAL_OK)
    {
        adcs_note_callback_error();
    }
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *i2c)
{
    HAL_StatusTypeDef status;
    uint16_t payload_length;

    if ((s_i2c == NULL) || (i2c != s_i2c))
    {
        return;
    }

    if (s_rx_phase == ADCS_RX_PHASE_COMMAND)
    {
        s_selected_command = s_command_byte;
        s_stats.last_command = s_command_byte;

        if ((s_command_byte == ADCS_I2C_CMD_GET_STATUS) ||
            (s_command_byte == ADCS_I2C_CMD_READ_FRAME))
        {
            s_rx_phase = ADCS_RX_PHASE_COMPLETE;
            s_stats.receive_complete_count++;
            adcs_set_event(ADCS_I2C_EVENT_ACTIVITY);
        }
        else if (s_command_byte == ADCS_I2C_CMD_WRITE_FRAME)
        {
            s_rx_active_length = ADCS_I2C_FRAME_HEADER_SIZE;
            s_rx_phase = ADCS_RX_PHASE_WRITE_HEADER;
            status = HAL_I2C_Slave_Seq_Receive_IT(
                i2c,
                s_rx_frames[s_rx_active_index],
                ADCS_I2C_FRAME_HEADER_SIZE,
                I2C_NEXT_FRAME
            );
            if (status != HAL_OK)
            {
                adcs_note_callback_error();
            }
        }
        else
        {
            s_rx_phase = ADCS_RX_PHASE_REJECTED;
            s_stats.invalid_command_count++;
            adcs_set_event(ADCS_I2C_EVENT_INVALID_COMMAND);
            __HAL_I2C_GENERATE_NACK(i2c);
        }
    }
    else if (s_rx_phase == ADCS_RX_PHASE_WRITE_HEADER)
    {
        payload_length = (uint16_t)(
            ((uint16_t)s_rx_frames[s_rx_active_index][2] << 8U) |
            s_rx_frames[s_rx_active_index][3]
        );

        if (payload_length > ADCS_I2C_MAX_FRAME_PAYLOAD)
        {
            s_rx_phase = ADCS_RX_PHASE_REJECTED;
            s_stats.malformed_frame_count++;
            adcs_set_event(ADCS_I2C_EVENT_MALFORMED_FRAME);
            __HAL_I2C_GENERATE_NACK(i2c);
        }
        else if (payload_length == 0U)
        {
            s_rx_phase = ADCS_RX_PHASE_COMPLETE;
            __HAL_I2C_GENERATE_NACK(i2c);
        }
        else
        {
            s_rx_active_length = (uint16_t)(
                ADCS_I2C_FRAME_HEADER_SIZE + payload_length
            );
            s_rx_phase = ADCS_RX_PHASE_WRITE_PAYLOAD;
            status = HAL_I2C_Slave_Seq_Receive_IT(
                i2c,
                &s_rx_frames[s_rx_active_index][ADCS_I2C_FRAME_HEADER_SIZE],
                payload_length,
                I2C_LAST_FRAME
            );
            if (status != HAL_OK)
            {
                adcs_note_callback_error();
            }
        }
    }
    else if (s_rx_phase == ADCS_RX_PHASE_WRITE_PAYLOAD)
    {
        s_rx_phase = ADCS_RX_PHASE_COMPLETE;
        __HAL_I2C_GENERATE_NACK(i2c);
    }
    else
    {
        adcs_note_callback_error();
    }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *i2c)
{
    if ((s_i2c == NULL) || (i2c != s_i2c))
    {
        return;
    }

    s_stats.transmit_complete_count++;
    if ((s_tx_kind == ADCS_TX_KIND_FRAME) &&
        s_tx_ready &&
        (s_tx_in_progress_index == s_tx_ready_index))
    {
        s_tx_ready = false;
        s_tx_ready_length = 0U;
    }

    s_tx_kind = ADCS_TX_KIND_NONE;
    s_selected_command = 0U;
    adcs_set_event(ADCS_I2C_EVENT_ACTIVITY);
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *i2c)
{
    HAL_StatusTypeDef status;

    if ((s_i2c == NULL) || (i2c != s_i2c))
    {
        return;
    }

    s_stats.listen_complete_count++;
    if ((s_rx_phase == ADCS_RX_PHASE_COMPLETE) &&
        (s_selected_command == ADCS_I2C_CMD_WRITE_FRAME))
    {
        /* STOP confirms that the master sent exactly the declared frame. */
        adcs_publish_received_frame();
    }
    adcs_note_malformed_receive();
    s_rx_phase = ADCS_RX_PHASE_IDLE;
    s_tx_kind = ADCS_TX_KIND_NONE;
    s_selected_command = 0U;

    status = HAL_I2C_EnableListen_IT(i2c);
    if (status == HAL_OK)
    {
        s_recovery_requested = false;
    }
    else
    {
        adcs_note_callback_error();
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c)
{
    if ((s_i2c == NULL) || (i2c != s_i2c))
    {
        return;
    }

    s_stats.error_count++;
    s_stats.last_error = HAL_I2C_GetError(i2c);
    if ((s_rx_phase == ADCS_RX_PHASE_COMPLETE) &&
        (s_selected_command == ADCS_I2C_CMD_WRITE_FRAME))
    {
        s_stats.malformed_frame_count++;
        adcs_set_event(ADCS_I2C_EVENT_MALFORMED_FRAME);
        s_rx_phase = ADCS_RX_PHASE_REJECTED;
    }
    else
    {
        adcs_note_malformed_receive();
    }
    s_recovery_requested = true;
    adcs_set_event(ADCS_I2C_EVENT_ERROR);
}
