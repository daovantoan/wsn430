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

#include "leds.h"

/**********************
 * Delay function.
 **********************/

#define DELAY 0x800

static void delay(unsigned int d)
{
  unsigned int i,j;
  for(j=0; j < 0xff; j++)
    {
      for (i = 0; i<d; i++)
	{
	  _no_operation();
	  _no_operation();
	}
    }
}

/**********************
 * Leds
 **********************/

int led_state;

static void led_change( void )
{
  LEDS_OFF();
  switch (led_state)
    {
    case 0: LED_RED_ON();   break;
    case 1: LED_GREEN_ON(); break;
    case 2: LED_BLUE_ON();  break;
    case 3: LEDS_ON();    break;
    }
  led_state = (led_state + 1) & 0x3;
}

/**********************
 *
 **********************/

int main(void)
{
  int i;

  // Stop the watchdog timer.
  WDTCTL = WDTPW | WDTHOLD;


  LEDS_INIT();
  LEDS_ON();
  delay(DELAY);
  LEDS_OFF();

  led_state = 0;

  while (1)
    {
      for (i=0; i<99; i++)
	{
	  led_change();
	  delay(DELAY >> 2);
	}
    }
}
