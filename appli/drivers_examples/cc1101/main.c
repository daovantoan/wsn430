/*
 * Copyright  2008-2009 SensTools, INRIA
 *
 * <dev-team@sentools.info>
 *
 * This software is a set of libraries designed to develop applications
 * for the WSN430 embedded hardware platform.
 *
 * This software is governed by the CeCILL license under French law and
 * abiding by the rules of distribution of free software.  You can  use,
 * modify and/ or redistribute the software under the terms of the CeCILL
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 * therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and,  more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license and that you accept its terms.
 */


#include <msp430.h>
#include <signal.h>
#include <stdio.h>

/* Project includes */
#include "clock.h"
#include "uart0.h"
#include "cc1101.h"
#include "leds.h"

static uint16_t rx_ok(void);
static uint16_t char_cb(uint8_t c);

int write(int file, char *ptr, int len);
int write(int file, char *ptr, int len)
{
    int i;
    file = file;
    for (i = 0; i < len; i++)
    {
        uart0_putchar(*ptr++);
    }
    return len;
}

uint8_t frame[128];
uint16_t frameseq = 0;
uint8_t length;
volatile uint16_t send = 0, receive = 0;

static int16_t compute_rssi(uint8_t raw) {
	int16_t rssi_d;
	printf("raw = %u\n", raw);
	if (raw >= 128) {
		rssi_d = raw;
		rssi_d -= 256;
		rssi_d -= 140;
	} else {
		rssi_d = raw;
		rssi_d -= 140;
	}
	return rssi_d;
}

/**
 * The main function.
 */
int main( void )
{
	/* Stop the watchdog timer. */
	WDTCTL = WDTPW + WDTHOLD;

	/* Setup MCLK 8MHz and SMCLK 1MHz */
	set_mcu_speed_xt2_mclk_8MHz_smclk_1MHz();

	/* Enable Interrupts */
	_no_operation();
	__enable_interrupt();

	uart0_init(UART0_CONFIG_1MHZ_115200);
	uart0_register_callback(char_cb);
	printf("CC1101 RXTX test program\r\n");

	LEDS_INIT();
	LEDS_OFF();

	cc1101_init();

	cc1101_cfg_append_status(CC1101_APPEND_STATUS_ENABLE);
	cc1101_cfg_crc_autoflush(CC1101_CRC_AUTOFLUSH_DISABLE);
	cc1101_cfg_white_data(CC1101_DATA_WHITENING_ENABLE);
	cc1101_cfg_crc_en(CC1101_CRC_CALCULATION_ENABLE);
	cc1101_cfg_freq_if(0x0C);

	cc1101_cfg_fs_autocal(CC1101_AUTOCAL_NEVER);
	cc1101_cfg_mod_format(CC1101_MODULATION_MSK);
	cc1101_cfg_sync_mode(CC1101_SYNCMODE_30_32);
	cc1101_cfg_manchester_en(CC1101_MANCHESTER_DISABLE);

	cc1101_cfg_txoff_mode(CC1101_TXOFF_MODE_IDLE);
	cc1101_cfg_rxoff_mode(CC1101_RXOFF_MODE_IDLE);

	// set channel bandwidth (560 kHz)
	cc1101_cfg_chanbw_e(0);
	cc1101_cfg_chanbw_m(2);

	// set data rate (0xD/0x2F is 250kbps)
	cc1101_cfg_drate_e(0x0D);
	cc1101_cfg_drate_m(0x2F);

	cc1101_cfg_chan(6);

	// Set the TX Power
	uint8_t table[] = {CC1101_868MHz_TX_12dBm};
	cc1101_cfg_patable(table, 1);
	cc1101_cfg_pa_power(0);

	printf("CC1101 initialized\r\nType 's' to send a message\r\n");

	while(1)
	{
		// Enter RX
		LED_RED_ON();
		cc1101_cmd_idle();
		cc1101_cmd_flush_rx();
		cc1101_cmd_calibrate();
		cc1101_cmd_rx();

		cc1101_cfg_gdo0(CC1101_GDOx_SYNC_WORD);
		cc1101_gdo0_int_set_falling_edge();
		cc1101_gdo0_int_clear();
		cc1101_gdo0_int_enable();
		cc1101_gdo0_register_callback(rx_ok);

		// Low Power Mode
		LPM0;

		// Check for send flag
		if (send == 1) {
			send = 0;
			LED_RED_OFF();
			cc1101_cmd_idle();
			cc1101_cmd_flush_tx();
			cc1101_cmd_calibrate();
			cc1101_gdo0_int_disable();

			frameseq ++;

			length = sprintf((char *)frame, "Hello World #%i", frameseq);

			printf("Sent : %s \r\n", frame);

			cc1101_fifo_put(&length, 1);
			cc1101_fifo_put(frame, length);

			cc1101_cmd_tx();

			// Wait for SYNC word sent
			while (cc1101_gdo0_read() == 0);

			// Wait for end of packet
			while (cc1101_gdo0_read() != 0);
		}

		// Check for receive flag
		if (receive == 1) {
			receive = 0;
			uint8_t i;

			// verify CRC result
			if ( !(cc1101_status_crc_lqi() & 0x80) ) {
				int16_t rssi;
				rssi = compute_rssi(cc1101_status_rssi());
				printf("bad crc, rssi = %i.%u\n", rssi>>1, 5* (rssi & 0x1));
				continue;
			}

			cc1101_fifo_get(&length, 1);

			if (length > 60) {
				continue;
			}

			cc1101_fifo_get(frame, length+2);

			//~ uint16_t rssi = (uint16_t)frame[length];
			int16_t rssi_d = compute_rssi(frame[length]);

			//~ if (rssi >= 128)
				//~ rssi_d = (rssi-256)-140;
			//~ else
				//~ rssi_d = rssi-140;

			printf("Frame received with RSSI=%d.%d dBm: ", rssi_d >> 1, 5*(rssi_d&0x1));
			for (i=0; i<length; i++) {
				printf("%c",frame[i]);
			}
			printf("\r\n");

			LED_GREEN_TOGGLE();
		}
	}

	return 0;
}

static uint16_t char_cb(uint8_t c) {
	if (c == 's') {
		send = 1;
		return 1;
	}

	return 0;
}

static uint16_t rx_ok(void) {
	receive = 1;
	return 1;
}
