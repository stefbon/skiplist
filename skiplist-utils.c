/*
  2010, 2011, 2012, 2013, 2014 Stef Bon <stefbon@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "global-defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <inttypes.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pthread.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "skiplist.h"
#include "logging.h"

void remove_sl_dirnode(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode)
{

    for (unsigned int i=0; i<dirnode->level; i++) {
	struct sl_dirnode_s *prev=dirnode->junction[i].p;
	struct sl_dirnode_s *next=dirnode->junction[i].n;

	prev->junction[i].step += dirnode->junction[i].step;
	prev->junction[i].n = next;
	next->junction[i].p = prev;

	sl->dirnode.junction[i].count--;

    }

}

void move_sl_dirnode_left(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, int difference, unsigned int *p_steps)
{

    while (difference>1) {
	unsigned int steps=0;
	unsigned int orig_steps=*p_steps;

	for (unsigned int i=0; i<dirnode->level; i++) {
	    struct sl_dirnode_s *prev=dirnode->junction[i].p;
	    void *data=dirnode->data;

	    dirnode->data=sl->ops.prev(data);
	    prev->junction[i].step--;
	    dirnode->junction[i].step++;
	    steps++;
	    if (orig_steps && (steps + 1 >= orig_steps)) break;

	}

	difference-=2; /* right increases with 1 and left decreases with 1: netto difference is 2 */
	*p_steps=steps;

    }

}

void move_sl_dirnode_right(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, int difference, unsigned int *p_steps)
{

    while (difference>1) {
	unsigned int steps=0;
	unsigned int orig_steps=*p_steps;

	for (unsigned int i=0; i<dirnode->level; i++) {
	    struct sl_dirnode_s *prev=dirnode->junction[i].p;
	    void *data=dirnode->data;

	    dirnode->data=sl->ops.next(data);
	    prev->junction[i].step++;
	    dirnode->junction[i].step--;
	    steps++;
	    if (orig_steps && (steps + 1 >= orig_steps)) break;

	}

	difference-=2; /* left increases with 1 and right decreases with 1: netto difference is 2 */
	*p_steps=steps;

    }

}

void _init_sl_dirnode(struct sl_dirnode_s *dirnode, unsigned short level, unsigned char flag)
{

    dirnode->data=NULL;
    dirnode->lock=0;
    dirnode->lockers=0;
    dirnode->flags|=flag;
    dirnode->level=level;

    for (unsigned int i=0; i<=dirnode->level; i++) {

	dirnode->junction[i].index=i;
	dirnode->junction[i].p=NULL;
	dirnode->junction[i].n=NULL;
	dirnode->junction[i].step=0;
	dirnode->junction[i].count=0;
    }

}

struct sl_dirnode_s *create_sl_dirnode(unsigned short level)
{
    unsigned int size=sizeof(struct sl_dirnode_s) + (level+1) * sizeof(struct sl_junction_s);
    struct sl_dirnode_s *dirnode=malloc(size);

    if (dirnode) {

	memset(dirnode, 0, size);
	_init_sl_dirnode(dirnode, level, _SKIPLIST_FLAG_ALLOC);

    }

    return dirnode;

}

unsigned short resize_sl_dirnode(struct sl_dirnode_s *dirnode, unsigned short level)
{
    unsigned int size = sizeof(struct sl_dirnode_s + (level+1) * sizeof(struct sl_junction_s));

    dirnode=(struct sl_dirnode_s *) realloc(dirnode, size);

    if (dirnode->level < level) {

	/* level increased: initialize the new headers  */

	memset(&dirnode->junction[dirnode->level], 0, (level - dirnode->level) * sizeof(struct sl_junction_s));
	for (unsigned int i=dirnode->level+1; i<=level; i++) header->head[i].index=i;
	header->level=level;

    }

    return level;

}


struct sl_vector_s *create_vector_path(unsigned short level)
{
    unsigned int size=sizeof(struct sl_vector_s) + (level+1) * sizeof(struct sl_path_s);
    struct sl_vector_s *vector=malloc(size);

    if (vector) {

	memset(vector, 0, size);
	for (unsigned int i=0; i<=level; i++) vector->path[i].index=i;
	vector->level=level;
	vector->minlevel=0;
	vector->maxlevel=level;

    }

    return vector;

}
