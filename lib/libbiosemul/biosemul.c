/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI doscmd.c,v 2.3 1996/04/08 19:32:30 bostic Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: projects/doscmd/doscmd.c,v 1.25 2002/03/07 12:52:26 obrien Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/specialreg.h>

#include <sys/proc.h>
#include <machine/sysarch.h>
#if 0
#include <machine/vm86.h>
#endif
#include <machine/vmm.h>
#include <vmmapi.h>
#include <assert.h>

#include "doscmd.h"
#include "tty.h"
#include "video.h"
#include "com.h"

#include <udis86/udis86.h>
/* exports */
int		capture_fd = -1;
int		dead = 0;
int		xmode = 0;
int		quietmode = 0;
int		booting = 0;
int		raw_kbd = 0;
int		timer_disable = 0;
struct timeval	boot_time;
u_int32_t	*ivec;
char		*lomem_addr;

#ifndef USE_VM86
#define PRB_V86_FORMAT  0x4242

struct vconnect_area vconnect_area = {
	0,				/* Interrupt state */
	PRB_V86_FORMAT,			/* Magic number */
	{ 0, },				/* Pass through ints */
	{ 0x00000000, 0x00000000 }	/* Magic iret location */
};
#endif

/* local prototypes */
#if 0
static void	setup_boot(regcontext_t *REGS);
#endif
static int	try_boot(int);
#if 0
static void	setup_command(int argc, char *argv[], regcontext_t *REGS);
static FILE	*find_doscmdrc(void);
static int	do_args(int argc, char *argv[]);
static void	usage(void);
static int	open_name(char *name, char *ext);

/* Local option flags &c. */
static int	zflag = 0;

/* DOS environment emulation */
static unsigned	ecnt = 0;
static char 	*envs[256];

/* Search path and command name */
static char	*dos_path = 0;
char		cmdname[256];	/* referenced from dos.c */

static struct vm86_init_args kargs;
#endif

static int set_modified_regs(struct vmctx *ctx, int vcpu, regcontext_t *orig, regcontext_t *modified);
static int get_all_regs(struct vmctx *ctx, int vcpu, regcontext_t *regs);

#define HDISK_CYL 95 
#define HDISK_HEAD 255
#define HDISK_TRACK 63
#define HDISK_FILE "/home/syuu/freebsd.img"

regcontext_t *saved_regcontext;
int trace_mode;
FILE *tracef;

/* lobotomise */
void biosemul_init(struct vmctx *ctx, int vcpu, char *lomem, int trace)
{
    int error;

    lomem_addr = lomem;
    ivec = (u_int32_t *)lomem_addr;

    init_ints();

    debugf = fopen("biosemul.log", "w");
    tracef = fopen("trace.log", "w");

    /* Call init functions */
#if 0
    if (raw_kbd)
	console_init();
#endif
    init_io_port_handlers();
    bios_init();
    init_hdisk(2, HDISK_CYL, HDISK_HEAD, HDISK_TRACK, HDISK_FILE, NULL);
    error = vm_set_register(ctx, 0, VM_REG_GUEST_RDX, 0x80);
    assert(error == 0);
    error = try_boot(booting = 2);	/* try C: */
    assert(error >= 0);
    cpu_init();
#if 0
    kbd_init();
    kbd_bios_init();
    video_init();
    if (xmode || quietmode)
	mouse_init();
    video_bios_init();
#endif
    disk_bios_init();
#if 0
    cmos_init();
    timer_init();
#endif
    /* iomap_init(); */
    init_com(0, NULL, 0x3f8, 4);

#if 0
    gettimeofday(&boot_time, 0);
#endif
 
 	trace_mode = trace;
 	if (trace_mode) {
 		uint64_t rflags;
 
 		error = vm_get_register(ctx, 0, VM_REG_GUEST_RFLAGS, &rflags);
 		assert(error == 0);
 		rflags |= 0x100; /* Trap Flag */
 		error = vm_set_register(ctx, 0, VM_REG_GUEST_RFLAGS, rflags);
 		assert(error == 0);
 	}
}

#if 0
/*
** setup_boot
**
** Setup to boot DOS
*/
static void
setup_boot(regcontext_t *REGS)
{
    int		fd;		/* don't close this! */

    fd = try_boot(booting = 2);		/* try C: */

    /* initialise registers for entry to bootblock */
    R_EFLAGS = 0x20202;
    R_CS = 0x0000;
    R_IP = 0x7c00;
    R_SS = 0x9800;
    R_SP = 0x8000 - 2;
    R_DS = 0x0000;
    R_ES = 0x0000;

    R_AX = R_BX = R_CX = R_DX = R_SI = R_DI = R_BP = 0;

#if defined(__FreeBSD__) || defined(__NetBSD__)
    /*
    ** init a few other context registers 
    */
    R_FS = 0x0000;
    R_GS = 0x0000;
#endif	
}
#endif

/*
** try_boot
**
** try to read the boot sector from the specified disk
*/
static int
try_boot(int bootdrv)
{
    int fd;

    fd = disk_fd(bootdrv);
    if (fd < 0)	{			/* can we boot it? */
	debug(D_DISK, "Cannot boot from %c\n", drntol(bootdrv));
	return -1;
    }
    
    /* read bootblock */
    if (read(fd, (char *)(lomem_addr + 0x7c00), 512) != 512) {
        debug(D_DISK, "Short read on boot block from %c:\n", drntol(bootdrv));
	return -1;
    }
    
    return fd;
}

#if 0
/*
** setup_command
**
** Setup to run a single command and emulate DOS
*/
static void
setup_command(int argc, char *argv[], regcontext_t *REGS)
{
    FILE	*fp;
    u_short	param[7] = {0, 0, 0, 0, 0, 0, 0};
    const char	*p;
    char	prog[1024];
    char	buffer[PATH_MAX];
    unsigned	i;
    int		fd;
    
    fp = find_doscmdrc();		/* dig up a doscmdrc */
    if (fp) {
	read_config(fp);		/* load config for non-boot mode */
	fclose(fp);
    }
    
    if (argc <= 0)			/* need some arguments */
	usage();

    /* look for a working directory  XXX ??? */
    if (dos_getcwd(drlton('C')) == NULL) {
	
	/* try to get our current directory, use '/' if desperate */
	p = getcwd(buffer, sizeof(buffer));
	if (!p || !*p) p = getenv("PWD");
	if (!p || !*p) p = "/";
	init_path(drlton('C'), "/", p);

	/* look for PATH= already set, learn from it if possible */
	for (i = 0; i < ecnt; ++i) {
	    if (!strncmp(envs[i], "PATH=", 5)) {
		dos_path = envs[i] + 5;
		break;
	    }
	}
	/* no PATH in DOS environment? put current directory there*/
	if (i >= ecnt) {
	    static char path[256];
	    snprintf(path, sizeof(path), "PATH=C:%s", dos_getcwd(drlton('C')));
	    put_dosenv(path);
	    dos_path = envs[ecnt-1] + 5;
	}
    }

    /* add a COMSPEC if required */
    for (i = 0; i < ecnt; ++i) {
	if (!strncmp(envs[i], "COMSPEC=", 8))
	    break;
    }
    if (i >= ecnt)
	put_dosenv("COMSPEC=C:\\COMMAND.COM");

    /* look for PATH already set, learn from it if possible */
    for (i = 0; i < ecnt; ++i) {
	if (!strncmp(envs[i], "PATH=", 5)) {
	    dos_path = envs[i] + 5;
	    break;
	}
    }
    /* No PATH, default to c:\ */
    if (i >= ecnt) {
	put_dosenv("PATH=C:\\");
	dos_path = envs[ecnt-1] + 5;
    }

    /* if no PROMPT, default to 'DOS>' */
    for (i = 0; i < ecnt; ++i) {
	if (!strncmp(envs[i], "PROMPT=", 7))
	    break;
    }
    if (i >= ecnt)
	put_dosenv("PROMPT=DOS> ");

    /* terminate environment */
    envs[ecnt] = 0;

    /* XXX ??? */
    if (dos_getcwd(drlton('R')) == NULL)
	init_path(drlton('R'), "/", 0);

    /* get program name */
    strncpy(prog, *argv++, sizeof(prog) -1);
    prog[sizeof(prog) -1] = '\0';

    /* try to open program */
    if ((fd = open_prog(prog)) < 0) {
	fprintf (stderr, "%s: command not found\n", prog);
	quit(1);
    }
    
    /* load program */
    load_command(REGS, 1, fd, cmdname, param, argv, envs);
    close(fd);
}

/*
** find_doscmdrc
**
** Try to find a doscmdrc file
*/
static FILE *
find_doscmdrc(void)
{
    FILE	*fp;
    char 	buffer[4096];
    
    if ((fp = fopen(".doscmdrc", "r")) == NULL) {
	struct passwd *pwd = getpwuid(geteuid());
	if (pwd) {
	    snprintf(buffer, sizeof(buffer), "%s/.doscmdrc", pwd->pw_dir);
	    fp = fopen(buffer, "r");
	}
	if (!fp) {
	    char *home = getenv("HOME");
            if (home) {
	        snprintf(buffer, sizeof(buffer), "%s/.doscmdrc", home);
                fp = fopen(buffer, "r");
	    }
        }
	if (!fp)
	    fp = fopen("/etc/doscmdrc", "r");
    }
    return(fp);
}

/*
** do_args
**
** commandline argument processing
*/
static int
do_args(int argc, char *argv[])
{
    int 	i,c,p;
    FILE	*fp;
    char 	*col;

    while ((c = getopt(argc, argv, "234AbCc:Dd:EFGHIi:kLMOo:PpQ:RrS:TtU:vVxXYz")) != -1) {
	switch (c) {
	case '2':
	    debug_flags |= D_TRAPS2;
	    break;
	case '3':
	    debug_flags |= D_TRAPS3;
	    break;
	case '4':
	    debug_flags |= D_DEBUGIN;
	    break;
	case 'A':
	    debug_flags |= D_TRAPS | D_ITRAPS;
	    for (c = 0; c < 256; ++c)
		debug_set(c);
	    break;
	case 'b':
	    booting = 1;
	    break;
	case 'C':
	    debug_flags |= D_DOSCALL;
	    break;
	case 'c':
	    if ((capture_fd = creat(optarg, 0666)) < 0) {
		perror(optarg);
		quit(1);
	    }
	    break;
	case 'D':
	    debug_flags |= D_DISK | D_FILE_OPS;
	    break;
	case 'd':
	    if ((fp = fopen(optarg, "w")) != 0) {
		debugf = fp;
		setbuf (fp, NULL);
	    } else
		perror(optarg);
	    break;
	case 'E':
	    debug_flags |= D_EXEC;
	    break;
	case 'F':
	    fossil = 1;
	    break;
	case 'G':
	    debug_flags |= D_VIDEO;
	    break;
	case 'H':
	    debug_flags |= D_HALF;
	    break;
	case 'I':
	    debug_flags |= D_ITRAPS;
	    for (c = 0; c < 256; ++c)
		debug_set(c);
	    break;
	case 'i':
	    i = 1;
	    if ((col = strchr(optarg, ':')) != 0) {
		*col++ = 0;
		i = strtol(col, 0, 0);
	    }
	    p = strtol(optarg, 0, 0);
	    iomap_port(p, i);

	    while (i-- > 0)
		define_input_port_handler(p++, inb_traceport);
	    break;
	case 'k':
            kargs.debug = 1;
	    break;
	case 'L':
	    debug_flags |= D_PRINTER;
	    break;
	case 'M':
	    debug_flags |= D_MEMORY;
	    break;
	case 'O':
	    debugf = stdout;
	    setbuf (stdout, NULL);
	    break;
	case 'o':
	    i = 1;
	    if ((col = strchr(optarg, ':')) != 0) {
		*col++ = 0;
		i = strtol(col, 0, 0);
	    }
	    p = strtol(optarg, 0, 0);
	    iomap_port(p, i);

	    while (i-- > 0)
		define_output_port_handler(p++, outb_traceport);
	    break;
	case 'P':
	    debug_flags |= D_PORT;
	    break;
	case 'p':
	    i = 1;
	    if ((col = strchr(optarg, ':')) != 0) {
		*col++ = 0;
		i = strtol(col, 0, 0);
	    }
	    p = strtol(optarg, 0, 0);
	    iomap_port(p, i);

	    while (i-- > 0) {
		define_input_port_handler(p++, inb_port);
		define_output_port_handler(p++, outb_port);
	    }
	    break;
	case 'Q':
	    quietmode = 1;
	    break;
	case 'R':
	    debug_flags |= D_REDIR;
	    break;
	case 'r':
	    raw_kbd = 1;
	    break;
	case 'S':
	    debug_flags |= D_TRAPS | D_ITRAPS;
	    debug_set(strtol(optarg, 0, 0));
	    break;
	case 'T':
	    timer_disable = 1;
	    break;
	case 't':
	    tmode = 1;
	    break;
	case 'U':
	    debug_unset(strtol(optarg, 0, 0));
	    break;
	case 'V':
	    vflag = 1;
	    break;
	case 'v':
	    debug_flags |= D_TRAPS | D_ITRAPS | D_HALF | 0xff;
	    break;
	case 'X':
	    debug_flags |= D_XMS;
	    break;
	case 'x':
#ifdef NO_X
	    fatal("X11 support not compiled in.\n");
#endif
	    xmode = 1;
	    break;
	case 'Y':
	    debug_flags |= D_EMS;
	    break;
	case 'z':
	    zflag = 1;
	    break;
	default:
	    usage ();
	}
    }
    return(optind);
}

/*
** Very helpful 8(
*/
void
usage (void)
{
	fprintf (stderr, "usage: doscmd cmd args...\n");
	quit (1);
}

/*
** look up a DOS command name
**
** XXX ordering is wrong!
*/
static int
open_name(char *name, char *ext)
{
    int fd;
    char *p = name + strlen(name);
    char *q;

    *ext = 0;

    q = strrchr(name, '/');
    if (q)
	q++;
    else
	q = name;

    if (!strchr(q, '.')) {
	strcpy(ext, ".exe");
	strcpy(p, ".exe");

	if ((fd = open (name, O_RDONLY)) >= 0)
	    return (fd);

	strcpy(ext, ".com");
	strcpy(p, ".com");

	if ((fd = open (name, O_RDONLY)) >= 0)
	    return (fd);
    } else {
	if ((fd = open (name, O_RDONLY)) >= 0)
	    return (fd);
    }

    return (-1);
}

/*
** look up a DOS command, search the path as well.
*/
int
open_prog(char *name)
{
    int fd;
    char fullname[1024], tmppath[1024];
    char *p;
    char *e;
    char ext[5];
    int error;
    int drive;
    char *path;

    if (strpbrk(name, ":/\\")) {
	error = translate_filename(name, fullname, &drive);
	if (error)
	    return (-1);

	fd = open_name(fullname, ext);

	strcpy(cmdname, name);
	if (*ext)
	    strcat(cmdname, ext);
	return (fd);
    }

    path = dos_path;

    while (*path) {
	p = path;
	while (*p && *p != ';')
	    ++p;

	memcpy(tmppath, path, p - path);
	e = tmppath + (p - path);
	*e++ = '\\';
	strcpy(e, name);

	path = *p ? p + 1 : p;

	error = translate_filename(tmppath, fullname, &drive);
	if (error)
	    continue;

	fd = open_name(fullname, ext);

	if (fd >= 0) {
	    strcpy(cmdname, tmppath);
	    if (*ext)
		strcat(cmdname, ext);
	    return (fd);
	}
    }

    return (-1);
}

/*
** append a value to the DOS environment
*/
void
put_dosenv(const char *value)
{
    if (ecnt < sizeof(envs)/sizeof(envs[0])) {
	if ((envs[ecnt++] = strdup(value)) == NULL) {
	    perror("put_dosenv");
	    quit(1);
	}
    } else {
	fprintf(stderr, "Environment full, ignoring %s\n", value);
    }
}
#endif

/*
** replicate a fd up at the top of the range
*/
int
squirrel_fd(int fd)
{
    int sfd = sysconf(_SC_OPEN_MAX);
    struct stat sb;

    do {
	errno = 0;
	fstat(--sfd, &sb);
    } while (sfd > 0 && errno != EBADF);

    if (errno == EBADF && dup2(fd, sfd) >= 0) {
	close(fd);
	return(sfd);
    }
    return(fd);
}

#if 0
/*
** Exit-time stuff
*/

/*
** Going away time
**
** XXX belongs somewhere else perhaps
*/
void
done(regcontext_t *REGS, int val)
{
    if (curpsp < 2) {
	if (xmode && !quietmode) {
	    const char *m;

	    tty_move(24, 0);
	    for (m = "END OF PROGRAM"; *m; ++m)
		tty_write(*m, 0x8400);

	    for (m = "(PRESS <CTRL-ALT> ANY MOUSE BUTTON TO exit)"; *m; ++m)
		tty_write(*m, 0x0900);
	    tty_move(-1, -1);
	    for (;;)
		tty_pause();
	} else {
	    quit(val);
	}
    }
    exec_return(REGS, val);
}
#endif

typedef struct COQ {
    void	(*func)(void *);
    void	*arg;
    struct COQ	*next;
} COQ;

COQ *coq = 0;

void
quit(int status)
{
    while (coq) {
	COQ *c = coq;
	coq = coq->next;
	c->func(c->arg);
    }
    if (!(xmode || quietmode))		/* XXX not for bootmode */
	puts("\n");
    exit(status);
}

void
call_on_quit(void (*func)(void *), void *arg)
{
    COQ *c = (COQ *)malloc(sizeof(COQ));
    if (!c) {
	perror("call_on_quit");
	quit(1);
    }
    c->func = func;
    c->arg = arg;
    c->next = coq;
    coq = c;
}

struct io_range {
	u_int start;
	u_int length;
	int enable;
};

/* This is commented out as it is never called.  Turn it back on if needed.
 */
#if COMMENTED_OUT
static void
iomap_init(void)
{
        int i;
        struct io_range io[] = {
#if 0
                { 0x200, 0x200, 1 },            /* 0x200 - 0x400 */
                { 0x1c80, 2, 1 },               /* 0x1c80 - 0x1c81 */
                { 0x2c80, 2, 1 },               /* 0x2c80 - 0x2c81 */
                { 0x3c80, 2, 1 },               /* 0x3c80 - 0x3c81 */
                { 0x378,  8, 1 },               /* 0x378 - 0x37F */
                { 0x3c4,  2, 1 },               /* 0x3c4 - 0x3c5 */
                { 0x3c5,  2, 1 },               /* 0x3ce - 0x3cf */
#else
		{ 0x0, 0x10000, 1 },		/* entire i/o space */
#endif
                { 0, 0, 0 }
        };
	
        for (i = 0; io[i].length; i++)
                if (i386_set_ioperm(io[i].start, io[i].length, io[i].enable) < 0)
                        err(1, "i386_set_ioperm");
}
#endif

#if 0
/* This is used to map in only the specified port range, instead of all
   the ports or only certain port ranges.
 */
void
iomap_port(int port, int count)
{
    if (i386_set_ioperm(port, count, 1) < 0)
	err(1, "i386_set_ioperm");

    debug(D_PORT,"mapped I/O port: port=%#x count=%d\n", port, count);
}
#endif

#define GET_REG(regname, vmmregname) \
{ \
	int error = 0; \
	reg86_t *r; \
	r = &regs->r.regname; \
	error = vm_get_register(ctx, vcpu, vmmregname, &r->r_rx); \
	assert(error == 0); \
}

#define GET_SEG(regname, vmmregname) \
{ \
	int error = 0; \
	uint32_t desc_limit, desc_access; \
	reg86_t *r; \
	r = &regs->r.regname; \
	error = vm_get_desc(ctx, vcpu, vmmregname, &r->r_rx, &desc_limit, \
	    &desc_access); \
	assert(error == 0); \
}

static int
get_all_regs(struct vmctx *ctx, int vcpu, regcontext_t *regs)
{
	GET_REG(gs, VM_REG_GUEST_GS);
	GET_REG(fs, VM_REG_GUEST_FS);
	GET_REG(es, VM_REG_GUEST_ES);
	GET_REG(ds, VM_REG_GUEST_DS);
	GET_REG(edi, VM_REG_GUEST_RDI);
	GET_REG(esi, VM_REG_GUEST_RSI);
	GET_REG(ebp, VM_REG_GUEST_RBP);
	GET_REG(ebx, VM_REG_GUEST_RBX);
	GET_REG(edx, VM_REG_GUEST_RDX);
	GET_REG(ecx, VM_REG_GUEST_RCX);
	GET_REG(eax, VM_REG_GUEST_RAX);
	GET_REG(eip, VM_REG_GUEST_RIP);
	GET_REG(cs, VM_REG_GUEST_CS);
	GET_REG(efl, VM_REG_GUEST_RFLAGS);
	GET_REG(esp, VM_REG_GUEST_RSP);
	GET_REG(ss, VM_REG_GUEST_SS);
	GET_REG(cr0, VM_REG_GUEST_CR0);
	GET_SEG(idtr, VM_REG_GUEST_IDTR);
	GET_SEG(gdtr, VM_REG_GUEST_GDTR);

	return (0);
}

#define SET_MODIFIED_REG(regname, vmmregname) \
{ \
	int error = 0; \
	reg86_t *r1, *r2; \
	r1 = &orig->r.regname; \
	r2 = &modified->r.regname; \
	if (r1->r_rx != r2->r_rx) { \
	    debug(D_ALWAYS, "%s " #regname ":%lx\n", __func__, r2->r_rx); \
	    error = vm_set_register(ctx, vcpu, vmmregname, r2->r_rx); \
	    assert(error == 0); \
	} \
}

static int
set_modified_regs(struct vmctx *ctx, int vcpu, regcontext_t *orig, regcontext_t *modified)
{
	SET_MODIFIED_REG(gs, VM_REG_GUEST_GS);
	SET_MODIFIED_REG(fs, VM_REG_GUEST_FS);
	SET_MODIFIED_REG(es, VM_REG_GUEST_ES);
	SET_MODIFIED_REG(ds, VM_REG_GUEST_DS);
	SET_MODIFIED_REG(edi, VM_REG_GUEST_RDI);
	SET_MODIFIED_REG(esi, VM_REG_GUEST_RSI);
	SET_MODIFIED_REG(ebp, VM_REG_GUEST_RBP);
	SET_MODIFIED_REG(ebx, VM_REG_GUEST_RBX);
	SET_MODIFIED_REG(edx, VM_REG_GUEST_RDX);
	SET_MODIFIED_REG(ecx, VM_REG_GUEST_RCX);
	SET_MODIFIED_REG(eax, VM_REG_GUEST_RAX);
	SET_MODIFIED_REG(eip, VM_REG_GUEST_RIP);
	SET_MODIFIED_REG(cs, VM_REG_GUEST_CS);
	SET_MODIFIED_REG(efl, VM_REG_GUEST_RFLAGS);
	SET_MODIFIED_REG(esp, VM_REG_GUEST_RSP);
	SET_MODIFIED_REG(ss, VM_REG_GUEST_SS);
	SET_MODIFIED_REG(cr0, VM_REG_GUEST_CR0);

	return (0);
}

extern void int13(regcontext_t *REGS);

void
exc_dump(regcontext_t *REGS)
{
	u_int32_t esp, exception, error_code, eip, cs, eflags, old_esp, ss, eip_off;
	ud_t ud_obj;

	ud_init(&ud_obj);
	ud_set_syntax(&ud_obj, UD_SYN_ATT);
	ud_set_vendor(&ud_obj, UD_VENDOR_INTEL);

	esp = R_ESP;
	exception = *(uint32_t *)(lomem_addr + esp);
	esp += 4;
	error_code = *(uint32_t *)(lomem_addr + esp);
	esp += 4;
	eip = *(uint32_t *)(lomem_addr + esp);
	esp += 4;
	cs = *(uint32_t *)(lomem_addr + esp);
	esp += 4;
	eflags = *(uint32_t *)(lomem_addr + esp);
	*(uint32_t *)(lomem_addr + esp) |= 0x100;
	esp += 4;
	if (cs & 0x3) {
		old_esp = *(uint32_t *)(lomem_addr + esp);
		esp += 4;
		ss =  *(uint32_t *)(lomem_addr + esp);
		eip_off = 0xa000;
	} else {
		old_esp = esp;
		ss = R_SS;
		eip_off = 0x0;
	}

	ud_set_mode(&ud_obj, 32);
	ud_set_pc(&ud_obj, eip);
	ud_set_input_buffer(&ud_obj, lomem_addr + eip + eip_off, 16);
	ud_disassemble(&ud_obj);

	fprintf(tracef, "32bit-%s exception:%d error_code:%x eip:%x cs:%x eflags:%x ss:%x esp:%x", 
		cs & 0x3 ? "user" : "kern", exception, error_code, eip, cs, eflags, ss, old_esp);
	fprintf(tracef, " insn:%s", ud_insn_asm(&ud_obj));
	fprintf(tracef, " ds:%x cr0:%x eax:%x ebx:%x ecx:%x edx:%x\n",
			R_DS, R_CR0, R_EAX, R_EBX, R_ECX, R_EDX);
	fprintf(stderr, "32bit-%s exception:%d error_code:%x eip:%x cs:%x eflags:%x ss:%x esp:%x", 
		cs & 0x3 ? "user" : "kern", exception, error_code, eip, cs, eflags, ss, old_esp);
	fprintf(stderr, " insn:%s", ud_insn_asm(&ud_obj));
	fprintf(stderr, " ds:%x cr0:%x eax:%x ebx:%x ecx:%x edx:%x\n",
			R_DS, R_CR0, R_EAX, R_EBX, R_ECX, R_EDX);

}

void
vmcall_dump(regcontext_t *REGS)
{
	ud_t ud_obj;
	uint32_t eip_off;

	ud_init(&ud_obj);
	ud_set_syntax(&ud_obj, UD_SYN_ATT);
	ud_set_vendor(&ud_obj, UD_VENDOR_INTEL);
	ud_set_mode(&ud_obj, R_CR0 & CR0_PE ? 32:16);
	ud_set_pc(&ud_obj, R_EIP);
	eip_off = (R_CR0 & CR0_PE && R_CS & 0x3) ? 0xa000 : 0;
	ud_set_input_buffer(&ud_obj, lomem_addr + R_EIP + eip_off, 16);
	ud_disassemble(&ud_obj);
	fprintf(tracef, "%dbit%s ip:%x cs:%x flags:%x sp:%x", 
		R_CR0 & CR0_PE ? 32:16,
		(R_CR0 & CR0_PE) ? (R_CS & 0x3) ? "-user" : "-kern" : "",
		R_EIP, R_CS, R_EFLAGS, R_ESP);
	fprintf(tracef, " insn:%s", ud_insn_asm(&ud_obj));
	fprintf(tracef, " ds:%x ss:%x cr0:%x eax:%x ebx:%x ecx:%x edx:%x\n",
		R_DS, R_SS, R_CR0, R_EAX, R_EBX, R_ECX, R_EDX);
	R_EFLAGS |= 0x100;
}


int
biosemul_call(struct vmctx *ctx, struct vm_exit *vmexit __unused, int vcpu)
{
	int ret = 0;
	regcontext_t orig, modified;
	regcontext_t *REGS = &modified;

	get_all_regs(ctx, vcpu, &orig);
	modified = orig;
	if (R_CR0 & CR0_PE) {
		switch (R_EIP) {
#if 0
		case 0x9198: /* ex_db */
			fprintf(tracef, "[trace] ");
			int01(&modified);
			break;
		case 0x919c: /* ex_noc */
			fprintf(tracef, "[ex_noc] ");
			fprintf(stderr, "[ex_noc] ");
			exc_dump(&modified);
			break;
#endif
		case 0x918f: /* except */
			fprintf(tracef, "[except] ");
			fprintf(stderr, "[except] ");
			exc_dump(&modified);
			break;
		case 0x9257: /* intx31 */
			fprintf(tracef, "[intx31] ");
			vmcall_dump(&modified);
			break;
		case 0x925c: /* int_hw */
			fprintf(tracef, "[int_hw] ");
			vmcall_dump(&modified);
			break;
		case 0x9402: /* intx30 */
			fprintf(tracef, "[intx30] ");
			vmcall_dump(&modified);
			break;
		default:
			fprintf(tracef, "[default] ");
			vmcall_dump(&modified);
			break;
		}
	} else if (R_IP > 0x400) { 
		fprintf(tracef, "[eip > 0x400] ");
		vmcall_dump(&modified);
	} else if (MAKEVEC(R_CS, R_IP) == ivec[0x01]) {
		fprintf(tracef, "[trace] ");
		int01(&modified);
	} else if (MAKEVEC(R_CS, R_IP) != ivec[0x01]) {
		callback_t func = find_callback(MAKEVEC(R_CS, R_IP));
		if (func)
			func(&modified);
		if (trace_mode) {
			fprintf(tracef, "[bioscall] ");
			int01(&modified);
			fprintf(tracef, "EFLAGS:%x\n", R_EFLAGS);
			R_EFLAGS |= 0x100;
		}
	}
	set_modified_regs(ctx, vcpu, &orig, &modified);
	if (trace_mode)
		fflush(tracef);

	return (ret);
}

bool biosemul_inout_registered(int in, int port)
{
	return io_port_defined(in, port);
}

int biosemul_inout(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	uint32_t *eax, int strict)
{
	regcontext_t orig, modified;

	get_all_regs(ctx, vcpu, &orig);
	modified = orig;
	saved_regcontext = &modified;

	debug(D_ALWAYS, "%s vcpu:%d in:%d port:%x bytes:%d\n",
	    __func__, vcpu, in, port, bytes);
	if (in)
		if (bytes == 1)
			inb(&modified, port);
		else if (bytes == 2)
			inw(&modified, port);
		else if (bytes == 4)
			inl(&modified, port);
		else
			fprintf(stderr, "%s invalid io bytes:%d\n",
				__func__, bytes);
	else
		if (bytes == 1)
			outb(&modified, port);
		else if (bytes == 2)
			outw(&modified, port);
		else if (bytes == 4)
			outl(&modified, port);
		else
			fprintf(stderr, "%s invalid io bytes:%d\n",
				__func__, bytes);
	set_modified_regs(ctx, vcpu, &orig, &modified);

	return 0;
}


