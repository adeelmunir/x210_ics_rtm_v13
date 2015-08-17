#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define	MAPM_ERR_MAP	((void *)-1)
#define	MAPM_ERR_FILE	((void *)-2)

#define mapm(a, l)		mapm_do(a, l, 1)
#define mapm_cc(a, l)	mapm_do(a, l, 0)

static void *fmapm(int fd, unsigned int addr, unsigned int len)
{
	unsigned char	*p;
	unsigned int	ofs;

	ofs = addr & (getpagesize() - 1);
	p = (unsigned char *)mmap(NULL, len + ofs, 
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr - ofs);
	if (p == MAP_FAILED)
	{
		return MAPM_ERR_MAP;
	}
	return p + ofs;
}

static int unmapm(void *addr, unsigned int len)
{
	unsigned int	ofs;

	ofs = (unsigned int)addr & (getpagesize() - 1);
	return munmap((void *)((unsigned int)addr - ofs), len + ofs);
}

static void *mapm_do(unsigned int addr, unsigned int len, int sync)
{
	int 	fd;
	void	*p;

	fd = open("/dev/mem", sync ? O_SYNC | O_RDWR : O_RDWR);
	if (fd == -1)
	{
		return MAPM_ERR_FILE;
	}
	p = fmapm(fd, addr, len);
	close(fd);
	return p;
}

int main(int argc, char * argv[])
{
	int base_addr = 0, nbytes = 64;
	int v_addr = 0;
	void * p;
	int i, size = 1;
	unsigned char linebuf[16], line_len;

	if(argc < 2)
	{
		printf("usage:\r\n    md [-b|-w|-l] address [-c count]\r\n");
		return (-1);
	}

	for(i=1; i<argc; i++)
	{
		if( !strcmp(argv[i],"-b") )
			size = 1;
		else if( !strcmp(argv[i],"-w") )
			size = 2;
		else if( !strcmp(argv[i],"-l") )
			size = 4;
		else if( !strcmp(argv[i],"-c") && (argc > i+1))
		{
			nbytes = strtoul(argv[i+1], NULL, 0);
			i++;
		}
		else if(*argv[i] == '-')
		{
			printf("md: invalid option '%s'\r\n", argv[i]);
			printf("usage:\r\n    md [-b|-w|-l] address [-c count]\r\n");
			return (-1);
		}
		else if(*argv[i] != '-' && strcmp(argv[i], "-") != 0)
		{
			base_addr = strtoul(argv[i], NULL, 0);
		}
	}

	if(size == 2)
	{
		base_addr = base_addr & (~0x00000001);
	}
	else if(size == 4)
	{
		base_addr = base_addr & (~0x00000003);
	}

	nbytes = nbytes * size;

	p = mapm(base_addr, nbytes);
	if(p == MAPM_ERR_FILE)
	{
		printf("open error\n");
		return -2;
	}
	if (p == MAPM_ERR_MAP)
	{
		printf(" map addr=0x%08x len=0x%08x error\n", base_addr, nbytes);
		return -3;
	}
	v_addr = (int)p;

	while(nbytes > 0)
	{
		line_len = (nbytes > 16) ? 16:nbytes;

		printf("%08x: ", base_addr);
		if(size == 1)
		{
			for(i=0; i<line_len; i+= size)
				*((unsigned char *)(&linebuf[i])) = *((unsigned char *)(v_addr+i));

			for(i=0; i<line_len; i+= size)
				printf(" %02x", *((unsigned char *)(&linebuf[i])));
		}
		else if(size == 2)
		{
			for(i=0; i<line_len; i+= size)
				*((unsigned short *)(&linebuf[i])) = *((unsigned short *)(v_addr+i));

			for(i=0; i<line_len; i+= size)
				printf(" %04x", *((unsigned short *)(&linebuf[i])));
		}
		else if(size == 4)
		{
			for(i=0; i<line_len; i+= size)
				*((unsigned int *)(&linebuf[i])) = *((unsigned int *)(v_addr+i));

			for(i=0; i<line_len; i+= size)
				printf(" %08x", *((unsigned int *)(&linebuf[i])));
		}

		printf("%*s", (16-line_len)*2+(16-line_len)/size+4, "");
		for(i=0; i<line_len; i++)
		{
			if( (linebuf[i] < 0x20) || (linebuf[i] > 0x7e) )
				printf(".");
			else
				printf("%c", linebuf[i]);
		}

		base_addr += line_len;
		v_addr += line_len;
		nbytes -= line_len;
		printf("\r\n");
	}

	unmapm(p, nbytes);
	return 0;
}

