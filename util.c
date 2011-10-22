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
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>             /* close() */


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
