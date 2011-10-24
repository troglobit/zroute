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

#ifndef ZROUTE_UTIL_H_
#define ZROUTE_UTIL_H_

#define DBG(fmt, args...) if (debug) fprintf (stderr, "%s:%s() - " fmt "\n", __progname, __func__, ##args)

#define POP_TOKEN()                   pop_token       (&opt, argc, argv, NULL)
#define POP_TOKEN_MATCH(keyword, str) pop_token_match (&opt, argc, argv, token, keyword, &str)

extern char   *__progname;

int            usage           (void);

char          *pop_token       (int *opt, int argc, char *argv[], char *keyword);
int            pop_token_match (int *opt, int argc, char *argv[], char *token, char *keyword, char **result);

int            zinet_ifindex   (char *ifname);
struct in_addr zinet_aton      (char *addr);
int            zinet_masktolen (struct in_addr mask);

#endif /* ZROUTE_UTIL_H_ */
