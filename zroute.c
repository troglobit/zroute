/* Simple client to add/del zebra static routes from the command line.
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

#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <poll.h>

/* Zebra includes */
#include <zebra.h>
#include <version.h>
#include <getopt.h>
#include <thread.h>
#include <stream.h>
#include <prefix.h>
#include <log.h>
#include <zclient.h>

/* Local includes. */
#include "util.h"

static int debug = 0;
extern char *__progname;
static const char *program_version = "zroute v" VERSION;
static const char *program_bug_address = "Joachim Nilsson <mailto:troglobit@gmail.com>";

/* All information about zebra. */
struct thread_master *master;	/* Workaround to allow linking with libzebra.a */

struct zclient *
setup_zebra_connection (void)
{
  int ret;
  struct zclient *zc;

  /* Dummy for when creating daemons with libzebra. */
  master = thread_master_create ();

  zc = zclient_new ();
  if (!zc)
    return NULL;

  zclient_init (zc, 0);
  zc->t_connect = NULL;
  ret = zclient_start (zc);
  if (ret)
    errx (1, "Failed zclient_start(), code %d\n", ret);

  return zc;
}

int
zroute (struct zclient *zclient, u_char op, struct in_addr network, u_char len, struct in_addr *nexthop, char *ifname, u_int32_t metric)
{
  unsigned int ifindex;
  struct zapi_ipv4 zr;
  struct prefix_ipv4 p;
  
  memset (&zr, 0, sizeof (zr));
  memset (&p, 0, sizeof (p));

  p.family = AF_INET;
  p.prefixlen = len;
  p.prefix = network;

  zr.type = ZEBRA_ROUTE_STATIC;
  zr.flags = 0;
  zr.message = 0;

  /* The ZAPI_MESSAGE_NEXTHOP flag must be set for both nexthop AND ifindex routes! */
  SET_FLAG (zr.message, ZAPI_MESSAGE_NEXTHOP);
  if (nexthop->s_addr != INADDR_ANY)
    {
      /* XXX: Possible to support >1, add that support! */
      zr.nexthop_num = 1;
      zr.nexthop     = &nexthop;
      DBG("Setting route via nexthop 0x%x", nexthop->s_addr);
    }

  if (ifname)
    {
      SET_FLAG (zr.message, ZAPI_MESSAGE_IFINDEX); /* XXX: Unused by zclient API? */
      zr.ifindex_num = 1;
      ifindex        = zinet_ifindex (ifname);
      zr.ifindex     = &ifindex;
      DBG("Setting route via ifname:%s => ifindex:%d", ifname, ifindex);
    }

  SET_FLAG (zr.message, ZAPI_MESSAGE_METRIC);
  zr.metric = metric;

  return zapi_ipv4_route (op, zclient, &p, &zr);
}

int
pend_zebra_reply (struct zclient *zclient)
{
  int ret;
  char dummy[420];
  struct pollfd fd = { 0, POLLIN | POLLPRI, 0 };

  /* Pend answer from zebra before exiting and stopping our zclient connection. */
  fd.fd = zclient->sock;
  ret = poll (&fd, 1, -1);
  do
    {
      ret = read (zclient->sock, dummy, sizeof (dummy));
      DBG("Zebra replied with %d bytes of data:", ret);
      if (ret > 0)
        {
          if (debug)
            write (STDERR_FILENO, dummy, ret);
        }
    }
  while (ret > 0);

  if (errno && EAGAIN != errno)
    return 1;

  DBG("Communication with Zebra OK.");

  return 0;
}

static int
usage (void)
{
  fprintf (stderr,
           "Usage: %s [OPTION] {add|del} TARGET NETMASK {GATEWAY|IFNAME}\n"
           "       %s [OPTION] {add|del} TARGET/LEN {GATEWAY|IFNAME}\n"
           "\n"
           " -v, --version             Display firmware version\n"
           " -V, --verbose             Verbose output, debug=0x9\n"
           " -?, --help                This help text.\n"
           "------------------------------------------------------------------------------\n"
           "The zroute tool is free software, thanks to the GNU General Public License.\n"
           "Copyright (C) 2011  %s\n\n",
           __progname, __progname, program_bug_address);

  return 1;
}

int
main (int argc, char **argv)
{
  int ret, op, len;
  struct in_addr net, gw;
  char *target = NULL, *netmask = NULL, *gateway = NULL, *ifname = NULL;
  struct zclient *zclient = NULL;
  struct option long_options[] = {
    {"verbose", 0, NULL, 'V'},
    {"version", 0, NULL, 'v'},
    {"help",    0, NULL, '?'},
    {NULL,      0, NULL,   0}
  };

  do
    {
      ret = getopt_long (argc, argv, "?hvVL", long_options, NULL);
      switch (ret)
        {
        case 'v':
          printf ("%s\n", program_version);
          return 0;

        case 'V':              /* Verbose */
          debug = 1;
          break;

        case ':':              /* Missing parameter for option. */
        case '?':              /* Unknown option. */
        case 'h':
        default:
          return usage ();

        case -1:               /* No more options. */
          break;
        }
    }
  while (ret != -1);

  DBG("optind:%d argc:%d", optind, argc);
  if (optind >= argc)
    return usage ();

  if (!strcmp (argv[optind], "del"))
    op = 1;
  else if (!strcmp (argv[optind], "add"))
    op = 0;
  else
    return usage ();
  optind++;

  DBG("op:%s", op ? "DEL" : "ADD");

  /* Gateway is interpreted as an outbound interface if it begins with
   * letter. Non-numeric gateways are not supported. */
  target = argv[optind++];
  netmask = strchr (target, '/');
  if (netmask)
    {
      *netmask = 0;
      len = atoi (++netmask);
      DBG("target:%s/%d", target, len);
    }
 else
   {
      netmask = argv[optind++];
      len = zinet_masktolen (zinet_aton (netmask));
      DBG("target:%s netmask %s => len %d", target, netmask, len);
   }

  gateway = argv[optind++];
  if (isalpha (gateway[0]))
    {
      ifname = gateway;
      gateway = NULL;
      DBG("ifname:%s", ifname);
    }

  if (gateway)
    {
      DBG("gateway:%s", gateway);
      gw = zinet_aton (gateway);
    }
  else
    {
      memset (&gw, 0, sizeof (gw));
    }
  net = zinet_aton (target);

  DBG("Connecting to Zebra routing daemon.");
  zclient = setup_zebra_connection ();
  if (!zclient || zclient->sock == -1)
    errx (1, "Failed connecting to zebra routing daemon.");

  DBG("Calling Zebra to %s route.", op ? "del" : "add");

  /* XXX: Hard coded metric, please add support when adding support for suboption markers.
   *      E.g., dev <IFNAME> metric <METRIC>, etc. */
  ret = zroute (zclient, op ? ZEBRA_IPV4_ROUTE_DELETE : ZEBRA_IPV4_ROUTE_ADD, net, len, &gw, ifname, 10);
  if (ret)
    errx (1, "Failed %s route, ret:%d\n", op ? "deleting" : "adding", ret);

  DBG("Awaiting Zebra reply...");
  ret = pend_zebra_reply (zclient);
  if (ret)
    warn ("No reply from zebra routing daemon");

  DBG("Disconnecting from Zeba.");
  zclient_stop (zclient);

  return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: nil
 *  c-file-style: "gnu"
 * End:
 */
