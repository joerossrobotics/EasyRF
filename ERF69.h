#pragma once

//
// RF69 transceiver minimalistic driver
//

#include <Arduino.h>
#include <SPI.h>

#define RF69_SPI_SETTINGS 8000000, MSBFIRST, SPI_MODE0
#define RF69_MODE_SWITCH_TOUT 100
#define RF69_PKT_SEND_TOUT    4000

#define RF69_KEY_LENGTH  16
#define RF69_MAX_MSG_SZ  64

typedef enum {
	rf_sleep,  // lowest power mode
	rf_idle,   // idle mode
	rf_fs,     // intermediate mode
	rf_tx,     // transmitting
	rf_rx      // receiving
} RF69_mode_t;

typedef enum {
	rf_PayloadReady = 1 << 2,
	rf_PacketSent   = 1 << 3,
} RF69_event_t;

typedef enum {
	rf_pw_normal = 0,
	// The following modes are for the high power modules only
	// They are using power amplifiers connected to separate pin of the chip (PA_BOOST)
	rf_pw_boost_normal,
	rf_pw_boost_high,
	rf_pw_boost_max,
} RF69_pw_mode_t;

struct RF69_config {
	// The carrier frequency
	uint32_t	freq_khz;
	// The bit transmission rate. Since we are using the default receiver filter width and
	// frequency deviation both equal to 5kHz, the baud rate should not exceed 10 Kbaud. You
	// will have to change a lot of default parameters in case you really need more.
	uint16_t	baud_rate;
	// Boost receiver sensitivity
	bool		rx_boost;
	// Transmitter power mode. We are using the default power settings that gives maximum power in each mode.
	RF69_pw_mode_t	tx_pw_mode;
};

class RF69 {
public:
	// Create transceiver object given the chip select and reset pins
	RF69(uint8_t cs_pin, uint8_t rst_pin)
		: m_cs_pin(cs_pin), m_rst_pin(rst_pin)
		, m_spi_settings(RF69_SPI_SETTINGS)
		{}

	// Initialize IO ports used for communications
	void   begin();
	// Check if transceiver is connected and powered on
	bool   probe();
	// Initialize transceiver. It makes hard reset first to have it clean.
	// This method must be called before any actions taken. It also may be
	// called to recover from fatal errors.
	void   init(struct RF69_config const* cfg);

	// Both communication devices must be initialized with the same network_id.
	// It provides the simple means for filtering garbage packets catch from the noise.
	void   set_network_id(uint32_t id);
	// Set encryption key (16 bytes long). Called with zero key pointer will clear current key.
	void   set_key(uint8_t const* key);

	//
	// The following methods will initiate transitions to different operating modes
	// and wait till transition completion. The may fail in case the transition does not
	// complete within predefined timeout. This typically means that transceiver becomes	
	// unresponsive and should be reinitialized.
	//
	bool   sleep()    { return switch_mode(rf_sleep); }
	bool   start_tx() { return switch_mode(rf_tx); }
	bool   start_rx() { return switch_mode(rf_rx); }
	bool   cancel()   { return switch_mode(rf_idle); }

	// Query current operating mode
	RF69_mode_t get_mode() { return (rd_reg(1) >> 2) & 7; }
	// Return the last set mode
	RF69_mode_t last_mode() { return m_flags.last_mode; }

	// Write packet to transceiver. Should be called in idle state.
	// The packet uses length prefix equals to the length of the message that follows.
	// The maximum message size is 64 bytes (65 bytes taking length prefix into account).
	// You have to call start_tx() to trigger actual packet transmission.
	void   wr_packet(uint8_t const* data) {
			wr_burst(0, data, 1 + *data);
		}
	// Read packet to the given buffer. If the packet does not fit to the buffer it will be truncated.
	void   rd_packet(uint8_t* buff, uint8_t buff_len) {
			rd_burst(0, buff, buff_len, true);
		}

	// Check if the packet was sent successfully in transmit mode.
	bool   packet_sent() { return get_events() & rf_PacketSent; }
	// Check if new packet was received in receive mode.
	bool   packet_rxed() { return get_events() & rf_PayloadReady; }
	// Wait particular event.
	bool   wait_event(RF69_event_t e, uint16_t tout);
	// Write packet and send it waiting for completion.
	bool   send_packet(uint8_t const* data) {
			wr_packet(data);
			return (start_tx() && wait_event(rf_PacketSent, RF69_PKT_SEND_TOUT));
		}

protected:
	void	reset();
	void    tx_begin();
	void    tx_end();
	uint8_t	tx_reg(uint16_t w);
	uint8_t rd_reg(uint8_t addr) {
			return tx_reg(addr << 8);
		}
	void    wr_reg(uint8_t addr, uint8_t val) {
			tx_reg(((0x80 | addr) << 8) | val);
		}
	void    rd_burst(uint8_t addr, uint8_t* buff, uint8_t buff_len, bool with_len_prefix = false);
	void    wr_burst(uint8_t addr, uint8_t const* data, uint8_t len);
	void    set_mode(RF69_mode_t m);
	bool    wait_mode(RF69_mode_t m, uint8_t tout = RF69_MODE_SWITCH_TOUT);
	bool    switch_mode(RF69_mode_t m) {
			set_mode(m); return wait_mode(m);
		}
	uint8_t get_events() { return rd_reg(0x28); }

private:
	uint8_t		m_cs_pin;
	uint8_t		m_rst_pin;
	SPISettings	m_spi_settings;
	struct {
		uint8_t	last_mode : 3;
		uint8_t	max_boost : 1;
	} m_flags;
};
