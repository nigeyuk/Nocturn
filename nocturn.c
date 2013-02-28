
/* Project Nocturn - High altitude balloon flight software               */
/*=======================================================================*/
/* Copyright 2012-2013 Philip Heron <phil@sanslogic.co.uk>               */
/*                     Nigel Smart <nigel@projectswift.co.uk>            */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include "rtty.h"
#include "gps.h"
#include "geofence.h"
#include "ds18x20.h"

#define LEDBIT(b) PORTB = (PORTB & (~_BV(2))) | ((b) ? _BV(2) : 0)

uint8_t id[2][8];

uint16_t crccat(char *msg)
{
	uint16_t x;
	for(x = 0xFFFF; *msg; msg++)
		x = _crc_xmodem_update(x, *msg);
	snprintf(msg, 8, "*%04X\n", x);
	return(x);
}

int main(void)
{
	uint32_t count = 0;
	int32_t lat, lon, alt, temp, tempe;
	uint8_t hour, minute, second;
	char msg[100];
	uint8_t i, r;
	
	/* Set the LED pin for output */
	DDRB |= _BV(DDB2);
	
	/* Turn on the LED (Debug only disbale for flight) */
	LEDBIT(1);
	
	rtx_init();
	
	sei();

	gps_setup();	
	
	/* Enable the radio and let it settle */
	rtx_enable(1);
	_delay_ms(2000);
	
	
	rtx_string_P(PSTR(RTTY_CALLSIGN " starting up\n"));	
	rtx_string_P(PSTR("Scanning 1-wire bus:\n"));
	
	for(i = 0; i < 3; i++)
	{
		r = ds_search_rom(id[i], i);
		
		if(r == DS_OK || r == DS_MORE)
		{
			rtx_wait();
			snprintf(msg, 100, "%i> %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n",
				i,
				id[i][0], id[i][1], id[i][2], id[i][3],
				id[i][4], id[i][5], id[i][6], id[i][7]);
			rtx_string(msg);
		}
		else
		{
			rtx_wait();
			snprintf(msg, 100, "%i> Error %i\n", i, r);
			rtx_string(msg);
		}
		
		if(r != DS_MORE) break;
	}
	rtx_string_P(PSTR("Done\n"));
	
	while(1)
	{

		/*Set the GPS navmode every 10 strings */
		if(count % 10 == 0)
		{

			/*mode 6 is "Airbourne with <1g Acceleration */
			if(gps_set_nav(6) != GPS_OK)
			{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",Error setting GPS navmode\n"));

			}

		}

			/*Get the latitude and longitude */
			if(gps_get_pos(&lat, &lon, &alt) != GPS_OK)
		{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",No or invalid GPS response\n"));
			lat = lon = alt = 0;
		}

			/*Get the GPS time*/
			if(gps_get_time(&hour, &minute, &second) != GPS_OK)
		{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",No or invalid GPS response\n"));
			hour = minute = second = 0;

		}


		/* Read the temperature from sensor 0 */
		ds_read_temperature(&temp, id[0]);
		
		ds_read_temperature(&tempe, id[1]);
		
		rtx_wait();
		
		snprintf(msg, 100, "$$$$%s,%li,%02i:%02i:%02i,%s%li.%05li,%s%li.%05li,%li,%li.%01li,%li.%01li,%c",
			RTTY_CALLSIGN, count++,
			hour, minute, second,
			(lat < 0 ? "-" : ""), labs(lat) / 10000000, labs(lat) % 10000000 / 100,
			(lon < 0 ? "-" : ""), labs(lon) / 10000000, labs(lon) % 10000000 / 100,
			alt / 1000,
			temp / 10000, labs(temp) / 1000 % 10,
			tempe / 10000, labs(tempe) / 1000 % 10,
			(geofence_uk(lat, lon) ? '1' : '0'));
		crccat(msg + 4);
		rtx_string(msg);
	}
}
