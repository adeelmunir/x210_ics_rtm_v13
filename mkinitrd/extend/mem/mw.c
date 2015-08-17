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
	unsigned int base_addr = 0, value = 0, c = 1;
	unsigned int index = 0, i, size = 1;
	unsigned int v_addr = 0;
	void * p;

	if(argc < 3)
	{
		printf("usage:\r\n    mw [-b|-w|-l] address value [-c count]\r\n");
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
			c = strtoul(argv[i+1], NULL, 0);
			if(c == 0)
			{
				printf("mw: the parmater of write count is zero by '-c %s'", argv[i+1]);
				return -1;
			}
			i++;
		}
		else if(*argv[i] == '-')
		{
			printf("mw: invalid option '%s'\r\n", argv[i]);
			printf("usage:\r\n    mw [-b|-w|-l] address value [-c count]\r\n");
			return (-1);
		}
		else if(*argv[i] != '-' && strcmp(argv[i], "-") != 0)
		{
			if(index == 0)
				base_addr = strtoul(argv[i], NULL, 0);
			else if(index == 1)
				value = strtoul(argv[i], NULL, 0);
			else if(index >= 2)
			{
				printf("mw: invalid paramter '%s'\r\n", argv[i]);
				return (-1);
			}
			index++;
		}
	}

	if(size == 2)
	{
		if(base_addr & 0x00000001)
		{
			base_addr = base_addr & (~0x00000001);
			printf("warnning: the address has been fixed to 0x%08x.\r\n", base_addr);
		}
	}
	else if(size == 4)
	{
		if(base_addr & 0x00000003)
		{
			base_addr = base_addr & (~0x00000003);
			printf("warnning: the address has been fixed to 0x%08x.\r\n", base_addr);
		}
	}

	c = c * size;

	p = mapm(base_addr, c);
	if(p == MAPM_ERR_FILE)
	{
		printf("open error\n");
		return -2;
	}
	if (p == MAPM_ERR_MAP)
	{
		printf(" map addr=0x%08x len=0x%08x error\n", base_addr, c);
		return -3;
	}
	v_addr = (unsigned int)p;

	for(i = 0; i < c; i+=size)
	{
		if(size == 1)
		{
			*((unsigned char *)(v_addr+i)) = value;
		}
		else if(size == 2)
		{
			*((unsigned short *)(v_addr+i)) = value;
		}
		else if(size == 4)
		{
			*((unsigned int *)(v_addr+i)) = value;
		}
	}
	printf("write done.\r\n");

	unmapm(p, c);
	return 0;
}

