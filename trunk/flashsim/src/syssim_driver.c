/*
 * DiskSim Storage Subsystem Simulation Environment (Version 3.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001, 2002, 2003.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */


/*
 * A sample skeleton for a system simulator that calls DiskSim as
 * a slave.
 *
 * Contributed by Eran Gabber of Lucent Technologies - Bell Laboratories
 *
 * Usage:
 *	syssim <parameters file> <output file> <max. block number>
 * Example:
 *	syssim parv.seagate out 2676846
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "syssim_driver.h"
#include "disksim_rand48.h"


#define	BLOCK	4096
#define	SECTOR	512
#define	BLOCK2SECTOR	(BLOCK/SECTOR)

typedef	struct	{
  int n;
  double sum;
  double sqr;
} Stat;


static SysTime now = 0;		/* current time */
static SysTime next_event = -1;	/* next event */
static int completed = 0;	/* last request was completed */
static Stat st;


void
panic(const char *s)
{
  perror(s);
  exit(1);
}


void
add_statistics(Stat *s, double x)
{
  s->n++;
  s->sum += x;
  s->sqr += x*x;
}


void
print_statistics(Stat *s, const char *title)
{
  double avg, std;

  avg = s->sum/s->n;
  std = sqrt((s->sqr - 2*avg*s->sum + s->n*avg*avg) / s->n);
  printf("%s: n=%d average=%f std. deviation=%f\n", title, s->n, avg, std);
}


/*
 * Schedule next callback at time t.
 * Note that there is only *one* outstanding callback at any given time.
 * The callback is for the earliest event.
 */
void
syssim_schedule_callback(void (*f)(void *,double), SysTime t)
{
  next_event = t;
}


/*
 * de-scehdule a callback.
 */
void
syssim_deschedule_callback(void (*f)())
{
  next_event = -1;
}


void
syssim_report_completion(SysTime t, Request *r)
{
  completed = 1;
  now = t;
  add_statistics(&st, t - r->start);
}

int
main(int argc, char *argv[])
{
  int i;
  int nsectors;
  struct stat buf;
  Request req;
  struct disksim *disksim, *disksim2;
  int len = 8192000;

  int test_encapsulation;
  char outfile2[81];

  //youkim
  FILE *fp = fopen(argv[5], "r");
  double time;
  int devno, bcount, flags;
  unsigned int blkno;
  char buffer[80];
  unsigned int cnt = 0;
  int BLOCKSIZE = 512;

  //if (argc != 5 || (nsectors = atoi(argv[3])) <= 0) {
  if (argc != 6 || (nsectors = atoi(argv[3])) <= 0) {
    //fprintf(stderr, "usage: %s <param file> <output file> <#sectors> <test_encapsulation>\n",
    fprintf(stderr, "usage: %s <param file> <output file> <#sectors> <test_encapsulation> <trace>\n", argv[0]);
    exit(1);
  }

  if (stat(argv[1], &buf) < 0)
    panic(argv[1]);

  test_encapsulation = atoi(argv[4]);

  if (test_encapsulation) {
    disksim2 = malloc (len);
    sprintf (outfile2, "%s2", argv[2]);
    disksim2 = disksim_interface_initialize(disksim2, len, argv[1], outfile2);

    /* NOTE: it is bad to use this internal disksim call from outside. */
    DISKSIM_srand48(2);
  }

  disksim = malloc (len);
  disksim = disksim_interface_initialize(disksim, len, argv[1], argv[2]);

  /* NOTE: it is bad to use this internal disksim call from external... */
  DISKSIM_srand48(1);


  //youkim
          
  while((fgets(buffer, sizeof(buffer), fp)) && (cnt < 10000)){  
    cnt++;
    sscanf(buffer, "%lf %d %ld %d %d\n", &time, &devno, &blkno, &bcount, &flags);

    req.start = now;
    req.type = (flags == 0)? 'W':'R';
    req.devno= 0;

    req.blkno = blkno;
    req.bytecount = BLOCKSIZE * bcount;
    completed = 0;

    disksim_interface_request_arrive(disksim, now, &req);

    /* Process events until this I/O is completed */
    while(next_event >= 0) {
      now = next_event;
      next_event = -1;
      disksim_interface_internal_event(disksim, now);
    }

    if (!completed) {
      fprintf(stderr,
	      "%s: internal error. Last event not completed %d\n",
	      argv[0], i);
      exit(1);
    }
  }
  fclose(fp);
  //youkim


  /*
  for (i=0; i < 1000; i++) {
    r.start = now;
    r.type = 'R';
    r.devno = 0;

    // NOTE: it is bad to use this internal disksim call from external... 
    r.blkno = BLOCK2SECTOR*(DISKSIM_lrand48()%(nsectors/BLOCK2SECTOR));
    r.bytecount = BLOCK;
    completed = 0;
    disksim_interface_request_arrive(disksim, now, &r);

    // Process events until this I/O is completed 
    while(next_event >= 0) {
      now = next_event;
      next_event = -1;
      disksim_interface_internal_event(disksim, now);
    }

    if (!completed) {
      fprintf(stderr,
	      "%s: internal error. Last event not completed %d\n",
	      argv[0], i);
      exit(1);
    }
  }
  */

  disksim_interface_shutdown(disksim, now);

  if (test_encapsulation) {
    disksim_interface_shutdown(disksim2, now);
  }

  print_statistics(&st, "response time");

  exit(0);
}
