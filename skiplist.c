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

int init_sl_skiplist(struct sl_skiplist_s *sl,
		    void *(* next)(void *data), void *(*prev)(void *data),
		    int (* compare) (void *a, void *b),
		    void *(* insert_before) (void *data, void *before, struct sl_skiplist_s *sl),
		    void *(* insert_after) (void *data, void *after, struct sl_skiplist_s *sl),
		    void (* delete) (void *data, struct sl_skiplist_s *sl),
		    unsigned int (* count) (struct sl_skiplist_s *sl),
		    void *(* first) (struct sl_skiplist_s *sl),
		    void *(* last) (struct sl_skiplist_s *sl),
		    unsigned int *p_error)
{
    unsigned int error=0;

    if ( ! sl) {

	error=EINVAL;

    } else if (! next || ! prev || ! compare || ! insert_before || ! insert_after || ! delete || ! count || ! first || ! last) {

	error=EINVAL;

    }

    if (error==0) {

	sl->ops.next=next;
	sl->ops.prev=prev;
	sl->ops.compare=compare;
	sl->ops.insert_before=insert_before;
	sl->ops.insert_after=insert_after;
	sl->ops.delete=delete;
	sl->ops.count=count;
	sl->ops.first=first;
	sl->ops.last=last;


	for (unsigned int i=0; i<=sl->maxlevel; i++) {

	    sl->dirnode.junction[i].n=&sl->dirnode;
	    sl->dirnode.junction[i].p=&sl->dirnode;

	}
	return 0;

    }

    *p_error=error;
    return -1;

}

struct sl_skiplist_s *create_sl_skiplist(unsigned char prob)
{
    unsigned int tmp=(prob==0) ? _SKIPLIST_PROB : prob;
    unsigned int size=sizeof(struct sl_skiplist_s) + (_SKIPLIST_MAXLANES + 2)  * sizeof(struct sl_junction_s));
    struct sl_skiplist_s *sl=malloc(size);

    if (sl) {

	memset(sl, 0, size);

	sl->prob=tmp;
	sl->flags=_SKIPLIST_FLAG_ALLOC;
	pthread_mutex_init(&sl->mutex, NULL);
	pthread_cond_init(&sl->cond, NULL);
	sl->maxlevel=_SKIPLIST_MAXLANES + 1;
	sl->dirnode.level=0;

	_init_sl_dirnode(&sl->dirnode, sl->maxlevel, _DIRNODE_FLAG_START);

    }

    return sl;

}

void clear_sl_skiplist(struct sl_skiplist_s *sl)
{
    struct sl_dirnode_s *dirnode=sl->dirnode.junction[0].n;

    while (dinode) {
	struct sl_dirnode_s *next=dirnode->junction[0].next; /* remind the next */

	free(dirnode);
	dirnode=next;

    }

}

void free_sl_skiplist(struct sl_skiplist_s *sl)
{
    clear_sl_skiplist(sl);
    pthread_mutex_destroy(&sl->mutex);
    pthread_cond_destroy(&sl->cond);
    if (sl->flag & _SKIPLIST_LAST_ALLOC) free(sl);
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
