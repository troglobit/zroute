/* zroute utility functions
 *
 * Copyright (c) 2011  Joachim Nilsson <troglobit@gmail.com>
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <arpa/inet.h>          /* inet_ntoa() */
#include <string.h>
#include <stdio.h>              /* fprintf() */
#include <stdlib.h>             /* exit() */
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>             /* close() */

#include "util.h"

/* Defined in zroute.c */
extern char *program_version;
extern char *program_bug_address;

int
usage (void)
{
  fprintf (stderr,
           "Usage: zroute [OPTION] {add|del} [-host|-net] TARGET [netmask NETMASK] [gw GATEWAY]\n"
           "                                                       [metric METRIC] [dev IFNAME]\n"
           "\n"
           "Where TARGET is one of     NET/PREFIXLEN | HOST | NET | default\n"
           "\n"
           " -v, --version             Display firmware version\n"
           " -V, --verbose             Verbose output, debug=0x9\n"
           " -?, --help                This help text.\n"
           "------------------------------------------------------------------------------\n"
           "The zroute tool is free software, thanks to the GNU General Public License.\n"
           "Copyright (C) 2011  %s\n\n", program_bug_address);

  return 1;
}

char *
pop_token (int *opt, int argc, char *argv[], char *keyword)
{
  int pos = *opt;
  char *token = NULL;

  if (pos < argc)
    {
      token = argv[pos++];
      *opt = pos;
    }

  if (!token)
    {
      if (keyword)
        fprintf (stderr, "Missing argument to keyword %s\n", keyword);
      usage ();
      exit (1);
    }

  return token;
}

int
pop_token_match (int *opt, int argc, char *argv[], char *token, char *keyword, char **result)
{
  int check = !strcmp (token, keyword);

  DBG("token:%s vs keyword:%s => check:%d", token, keyword, check);
  if (check)
    {
      *result = pop_token (opt, argc, argv, keyword);
    }

  return check;
}

int
zinet_ifindex (char *ifname)
{
   int sd;
   struct ifreq ifr;

   sd = socket (PF_PACKET, SOCK_RAW, 0);
   if (sd < 0)
      return -1;

   memset (&ifr, 0, sizeof (ifr));
   strncpy (ifr.ifr_name, ifname, strlen (ifname));
   ifr.ifr_ifindex = -1;
   ioctl (sd, SIOCGIFINDEX, &ifr);
   close (sd);

   return ifr.ifr_ifindex;
}

int
zinet_masktolen (struct in_addr mask)
{
   int len = 0;
   int hostmask = ntohl (mask.s_addr);

   while (hostmask)
   {
      hostmask <<= 1;
      len ++;
   }

   return len;
}

struct in_addr
zinet_aton (char *addr)
{
  struct in_addr inet;

  inet_pton (AF_INET, addr, &inet);

  return inet;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: nil
 *  c-file-style: "gnu"
 * End:
 */
