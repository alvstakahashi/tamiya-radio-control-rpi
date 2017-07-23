// 本ソースコードは、
// 大金システム設計事務所 Home Page: http://www.ogane.com/
// http://homebrew.jp/show?page=1455
// より、了解を得て、利用しています。
// ライセンスは、GPLv2とします。
//\par Open Source Licensing GPL V2
//
//  This is the appropriate option if you want to share the source code of your
//  application with everyone you distribute it to, and you also want to give them
//  the right to share who uses it. If you wish to use this software under Open
//  Source Licensing, you must contribute all your source code to the open source
//  community in accordance with the GPL Version 2 when your application is
//  distributed. See http://www.gnu.org/copyleft/gpl.html and COPYING

#include "bcm2835.h"
#include <unistd.h>

#include <kernel.h>
#include "kernel_cfg.h"

#include "rpi_lib/rpi.h"
#include <stdio.h>
#include <stdint.h>


#define max(a, b)       (a >= b ? a : b)
#define min(a, b)       (a <= b ? a : b)

#define PIN RPI_GPIO_P1_11

extern  volatile uint32_t *bcm2835_gpio;
int 	pulseWidth;			//リアモーターパルス幅　単位10us

int servo_main(int duty)
{
    int pw = 50;

    printf("servo main duty = %d \n",duty);
    pw = duty;
    pw = min(100, max(0, pw));
    pw = 1000 + (int)((1000 * pw) / 100);

    pulseWidth = pw/10;

    printf("pulse width %d usec, duration forever\n", pw);

    if (!bcm2835_init())
        return 1;

    printf("bcm2835_gpio=%08x\n",bcm2835_gpio);

    bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);

	sta_cyc(MAIN_CYC);
    return 0;
}
void cyclic_handler(intptr_t exinf)
{
	bcm2835_gpio_write(PIN, HIGH);
	ista_alm(MAIN_ALM,pulseWidth);
	digitalWrite(16, LOW);
	iact_tsk(TASK1_ID);
}
void alarm_handler(intptr_t exinf)
{
	bcm2835_gpio_write(PIN, LOW);
}
