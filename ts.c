//gcc -fno-tree-cselim -Wall -O0 -mcpu=arm9 -o ts ts.c gpiolib.c fpga.c

#include <stdio.h>
#include <unistd.h>
#include <dirent.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <assert.h>

#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/types.h>
#include <math.h>

#include "fpga.h"
#include "gpiolib.h"
#include "crossbar-ts7680.h"

static int twifd;


#include "i2c-dev.h"

int fpga_init(char *path, char adr)
{
	static int fd = -1;

	if(fd != -1)
		return fd;

	if(path == NULL) {
		// Will always be I2C0 on the 7680
		fd = open("/dev/i2c-0", O_RDWR);
	} else {
		
		fd = open(path, O_RDWR);
	}

	if(!adr) adr = 0x28;

	if(fd != -1) {
		if (ioctl(fd, I2C_SLAVE_FORCE, 0x28) < 0) {
			perror("FPGA did not ACK 0x28\n");
			return -1;
		}
	}

	return fd;
}

void fpoke8(int twifd, uint16_t addr, uint8_t value)
{
	uint8_t data[3];
	data[0] = ((addr >> 8) & 0xff);
	data[1] = (addr & 0xff);
	data[2] = value;
	if (write(twifd, data, 3) != 3) {
		perror("I2C Write Failed");
	}
}

uint8_t fpeek8(int twifd, uint16_t addr)
{
	uint8_t data[2];
	data[0] = ((addr >> 8) & 0xff);
	data[1] = (addr & 0xff);
	if (write(twifd, data, 2) != 2) {
		perror("I2C Address set Failed");
	}
	read(twifd, data, 1);

	return data[0];
}

int get_model()
{
	FILE *proc;
	char mdl[256];
	char *ptr;

	proc = fopen("/proc/device-tree/model", "r");
	if (!proc) {
		perror("model");
		return 0;
	}
	fread(mdl, 256, 1, proc);
	ptr = strstr(mdl, "TS-");
	return strtoull(ptr+3, NULL, 16);
}

void usage(char **argv) {
	fprintf(stderr,
		"Usage: %s [OPTIONS] ...\n"
		"Technologic Systems I2C FPGA Utility\n"
		"\n"
		"  -i, --info             Display board info\n"
		"  -m, --addr <address>   Sets up the address for a peek/poke\n"
		"  -v, --poke <value>     Writes the value to the specified address\n"
		"  -t, --peek             Reads from the specified address\n"
		"\n",
		argv[0]
	);
}

int main(int argc, char **argv) 
{
	int c, i;
	uint16_t addr = 0x0;
	int opt_addr = 0;
	int opt_poke = 0, opt_peek = 0, opt_auto485 = -1;
	int opt_set = 0, opt_get = 0, opt_dump = 0;
	int opt_info = 0, opt_setmac = 0, opt_getmac = 0;
	int opt_cputemp = 0, opt_modbuspoweron = 0, opt_modbuspoweroff = 0;
	int opt_dac0 = 0, opt_dac1 = 0, opt_dac2 = 0, opt_dac3 = 0;
	char *opt_mac = NULL;
	int baud = 0;
	int model;
	uint8_t pokeval = 0;
	char *uartmode = 0;
	struct cbarpin *cbar_inputs, *cbar_outputs;
	int cbar_size, cbar_mask;

	static struct option long_options[] = {
		{ "addr", 1, 0, 'm' },
		{ "address", 1, 0, 'm' },
		{ "poke", 1, 0, 'v' },
		{ "peek", 1, 0, 't' },
		{ "pokef", 1, 0, 'v' },
		{ "peekf", 1, 0, 't' },
		{ 0, 0, 0, 0 }
	};
	model = get_model();
	if(model == 0x7680) {
		cbar_inputs = ts7680_inputs;
		cbar_outputs = ts7680_outputs;
		cbar_size = 6;
		cbar_mask = 3;
	} else {
		fprintf(stderr, "Unsupported model TS-%X\n", model);
		return 1;
	}

	while((c = getopt_long(argc, argv, "+m:v:t:",
	  long_options, NULL)) != -1) {
		switch(c) {

		case 'm':
			opt_addr = 1;
			addr = strtoull(optarg, NULL, 0);
			break;
		case 'v':
			opt_poke = 1;
			pokeval = strtoull(optarg, NULL, 0);
			break;
		case 't':
			addr = strtoull(optarg, NULL, 0);
      			opt_peek = 1;
			break;
		default:
			usage(argv);
			return 1;
		}
	}
  
  twifd = fpga_init(NULL, 0);
	if(twifd == -1) {
		perror("Can't open FPGA I2C bus");
		return 1;
	}

	if(opt_poke) {
		fpoke8(twifd, addr, pokeval);
	}

	if(opt_peek) {
		printf("0x%X\n", fpeek8(twifd, addr));
	}

	close(twifd);

	return 0;
}
