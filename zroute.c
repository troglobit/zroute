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

/* System includes */
#include <err.h>
#include <poll.h>

/* Quagga includes */
#include <quagga/zebra.h>
#include <quagga/thread.h>
#include <quagga/prefix.h>
#include <quagga/zclient.h>

/* Local includes. */
#include "util.h"

#define ZROUTE_METRIC_DEFAULT 0

int debug = 0;
const char *program_version = "zroute v" VERSION;
const char *program_bug_address = "Joachim Nilsson <mailto:troglobit@gmail.com>";

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
  if (nexthop && nexthop->s_addr != INADDR_ANY)
    {
      /* XXX: Possible to support >1, add that support! */
      zr.nexthop_num = 1;
      zr.nexthop     = &nexthop;
      DBG("via nexthop 0x%x", nexthop->s_addr);
    }

  if (ifname)
    {
      SET_FLAG (zr.message, ZAPI_MESSAGE_IFINDEX); /* XXX: Unused by zclient API? */
      zr.ifindex_num = 1;
      ifindex        = zinet_ifindex (ifname);
      zr.ifindex     = &ifindex;
      DBG("via ifname:%s => ifindex:%d", ifname, ifindex);
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
          if (debug && -1 == write (STDERR_FILENO, dummy, ret))
            DBG("Failed writing return data from Zebra to stderr.");
        }
    }
  while (ret > 0);

  if (errno && EAGAIN != errno)
    return 1;

  DBG("Communication with Zebra OK.");

  return 0;
}

int
main (int argc, char *argv[])
{
  int opt = 1, ret, len = -1, metric = ZROUTE_METRIC_DEFAULT;
  u_char op = 0;
  struct in_addr net, gw;
  char *token, *ptr;
  char *target = NULL, *netmask = NULL, *gateway = NULL, *ifname = NULL;
  struct zclient *zclient = NULL;

  memset (&gw, 0, sizeof (gw));
  memset (&net, 0, sizeof (net));

  if (argc < 2)
    return usage ();

  while (opt < argc)
    {
      token = POP_TOKEN();

      if (!strcmp (token, "-V") || !strcmp (token, "--verbose"))
        {
          debug = 1;
          continue;
        }

      if (!strcmp (token, "-v") || !strcmp (token, "--version"))
        {
          printf ("%s\n", program_version);
          return 0;
        }

      if (!strcmp (token, "-h") || !strcmp (token, "--help"))
        {
          return usage ();
        }

      if (!op && (!strcmp (token, "add") || !strcmp (token, "del")))
        {
          if (!strcmp (token, "add"))
            op = ZEBRA_IPV4_ROUTE_ADD;
          else
            op = ZEBRA_IPV4_ROUTE_DELETE;

          token = POP_TOKEN();
          if (!strcmp (token, "default"))
            {
              target = "0.0.0.0";
              len = 0;
            }
          else
            {
              if (POP_TOKEN_MATCH("-net", target))
                {
                  /* Do nothing, netmask or /LEN required. */
                }
              else if (POP_TOKEN_MATCH("-host", target))
                {
                  len = 32;
                }
              else
                {
                  /* Neigher -net or -host */
                  target = token;
                }

              ptr = strchr (target, '/');
              if (ptr)
                {
                  *ptr = 0;
                  len = atoi (++ptr);
                }
            }

          net = zinet_aton (target);
          DBG("op:%s target:%s/%d", op == ZEBRA_IPV4_ROUTE_ADD ? "ADD" : "DEL", target, len);
          continue;
        }

      if (POP_TOKEN_MATCH("netmask", netmask))
        {
          len = zinet_masktolen (zinet_aton (netmask));
          DBG("target:%s netmask %s => len %d", target, netmask, len);
          continue;
        }

      if (POP_TOKEN_MATCH("gw", gateway))
        {
          gw = zinet_aton (gateway);
          DBG("gw:%s(0x%x)", gateway, gw.s_addr);
          continue;
        }

      if (POP_TOKEN_MATCH("dev", ifname))
        {
          DBG("dev:%s", ifname);
          continue;
        }

      if (POP_TOKEN_MATCH("metric", token))
        {
          metric = atoi (token);
          DBG("metric:%d", metric);
          continue;
        }

      return usage ();
    }

  DBG("Connecting to Zebra routing daemon.");
  zclient = setup_zebra_connection ();
  if (!zclient || zclient->sock == -1)
    errx (1, "Failed connecting to zebra routing daemon.");

  DBG("Calling Zebra to %s route.", op ? "del" : "add");
  ret = zroute (zclient, op, net, len, gateway ? &gw : NULL, ifname, metric);
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
