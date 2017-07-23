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


#include <kernel.h>
#include "kernel_cfg.h"

#include "rpi_lib/rpi.h"
#include <stdio.h>
#include <stdint.h>


#undef GPIO_BASE
#undef PWM_BASE
#undef CLOCK_BASE

#define BCM2708_PERI_BASE       0x20000000
#define GPIO_BASE               (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define PWM_BASE                (BCM2708_PERI_BASE + 0x20C000) /* PWM controller */
#define CLOCK_BASE              (BCM2708_PERI_BASE + 0x101000)

#define PWM_CTL  0
#define PWM_RNG1 4
#define PWM_DAT1 5

#define PWMCLK_CNTL 40
#define PWMCLK_DIV  41

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef NOT_BAREMATAL
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

#define max(a, b)       (a >= b ? a : b)
#define min(a, b)       (a <= b ? a : b)

#ifndef NOT_BAREMETAL
#define usleep	delayMicroseconds
#endif


// I/O access
volatile unsigned *gpio;
volatile unsigned *pwm;
volatile unsigned *clk;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0


extern int auto_steer;

void setServo(int);
void initHardware();

int pwm_main(int param_pw,int param_dur,int auto_prm)
{
	stp_alm(SUB_ALM);
    int pw = 50;
    int dur = -1;
    pw = param_pw;
    pw = min(100, max(0, pw));

	if (auto_prm)
	{
		auto_steer = TRUE;
	}
	else
	{
		auto_steer = FALSE;
	}
   	if(param_dur  > 0)
    {
		dur = param_dur;
	}
	if(dur > 0)
    {
		printf("pulse width %d%%, duration %d msec\n", pw, dur);
    }
    else
    {
		printf("pulse width %d%%, duration forever\n", pw);
	}
    // init PWM module for GPIO pin 18 with 50 Hz frequency
    initHardware();

    setServo(pw);
	sta_alm(SUB_ALM,dur*100);
}

void alarm_handler2(intptr_t exinf)
{
	int pw;
	if (auto_steer)
	{
	       auto_steer = FALSE;
	       pw = min(100, max(0, 60));
	       setServo(pw);
	       ista_alm(SUB_ALM,20000);
	       return;
	}	
    *(clk + PWMCLK_CNTL) = 0x5A000000 | (1 << 5);
	printf("alarm_handler2 here\n"); 

    usleep(10);
    *(pwm + PWM_CTL) = 0;
    GPIO_SET = 1;
    return ;
}

// map 4k register memory for direct access from user space and return a user space pointer to it
volatile unsigned *mapRegisterMemory(int base)
{
#ifdef NOT_BAREMATAL
        static int mem_fd = 0;
        char *mem, *map;

        /* open /dev/mem */
        if (!mem_fd) {
                if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
                        printf("can't open /dev/mem \n");
                        exit (-1);
                }
        }

        /* mmap register */

        // Allocate MAP block
        if ((mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
                printf("allocation error \n");
                exit (-1);
        }

        // Make sure pointer is on 4K boundary
        if ((unsigned long)mem % PAGE_SIZE)
                mem += PAGE_SIZE - ((unsigned long)mem % PAGE_SIZE);

        // Now map it
        map = (char *)mmap(
                (caddr_t)mem,
                BLOCK_SIZE,
                PROT_READ|PROT_WRITE,
                MAP_SHARED|MAP_FIXED,
                mem_fd,
                base
        );

        if ((long)map < 0) {
                printf("mmap error %d\n", (int)map);
                exit (-1);
        }

        // Always use volatile pointer!
        return (volatile unsigned *)map;
#else
        printf("mapaddress == %08x\n",base);
        return (volatile unsigned *)base;
#endif
}

// set up a memory regions to access GPIO, PWM and the clock manager
void setupRegisterMemoryMappings()
{
	gpio = mapRegisterMemory(GPIO_BASE);
    pwm = mapRegisterMemory(PWM_BASE);
    clk = mapRegisterMemory(CLOCK_BASE);
}

void setServo(int percent)
{
    unsigned int bits = 0;

	bits = 45 + percent/2;
	printf("set servo bits = %d\n",bits);
    *(pwm + PWM_DAT1) = bits;
}

// init hardware
void initHardware()
{
        // mmap register space
        setupRegisterMemoryMappings();

        // set PWM alternate function for GPIO18
        SET_GPIO_ALT(18, 5);

        // stop clock and waiting for busy flag doesn't work, so kill clock
        *(clk + PWMCLK_CNTL) = 0x5A000000 | (1 << 5);
        usleep(10);

        // set frequency
        *(clk + PWMCLK_DIV)  = 0x5A000000 | (400 <<12);

        // source=osc and enable clock
        *(clk + PWMCLK_CNTL) = 0x5A000011;

        // disable PWM
        *(pwm + PWM_CTL) = 0;

        // needs some time until the PWM module gets disabled, without the delay the PWM module crashs
        usleep(10);

        *(pwm + PWM_RNG1) = 1024;

		*(pwm + PWM_CTL) = 0x5A000081;		// MSモードで利用
}
