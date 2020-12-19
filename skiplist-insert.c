/*
  2010, 2011, 2012, 2013 Stef Bon <stefbon@gmail.com>

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

#include <math.h>

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "skiplist.h"
#include "skiplist-find.h"
#include "skiplist-insert.h"
#include "logging.h"

/*

    determine the level of a new dirnode when an entry is added

    this function tests the number of dirnodes is sufficient when one entry is added

    return:

    level=-1: no dirnode required
    level>=0: a dirnode required with level, 
    it's possible this is +1 the current level, in that case an extra lane is required

*/

static int do_levelup(struct skiplist_struct *sl)
{
    int level=-1;
    unsigned hlp=0;

    pthread_mutex_lock(&sl->mutex);

    hlp=sl->ops.count(sl);

    if ( hlp > sl->prob ) {
	struct sl_dirnode_s *dirnode=sl->dirnode;
	unsigned ctr=0;

	while (ctr<=dirnode->level) {

	    hlp=hlp / sl->prob;

	    /*
		    in ideal case is the number of dirnodes per lane

		    hlp == dn_count[ctr] + 1

		    plus one cause the head dirnode

		    in any case it's required to add a dirnode in this lane when

		    hlp > dn_count[ctr] + 1

	    */

	    if (hlp < dirnode->junction[ctr].count + 1) break;
	    level=ctr;
	    ctr++;

	}

	if (level==dirnode->level) {

	    /* test an extra lane is required */

	    if ( hlp > sl->prob ) level++;

	}

    } else {

	level=0;

    }

    unlock:

    pthread_mutex_unlock(&sl->mutex);

    return level;

}

static struct sl_dirnode_s *insert_sl_dirnode(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, void *data, unsigned int left, unsigned int right)
{
    struct sl_dirnode_s *new=NULL;
    int newlevel=do_levelup(sl);

    if (newlevel>=0) {

	if (newlevel>sl->maxlevel) newlevel=sl->maxlevel;

	new=create_sl_dirnode(newlevel);

	if (new) {
	    struct sl_dirnode_s *prev=dirnode;
	    struct sl_dirnode_s *next=dirnode->junction[0].n;

	    for (unsigned int i=0; i<newlevel; i++) {

		while (prev->level < i) {

		    prev=prev->junction[i].p;
		    left+=prev->junction[i].step;

		}

		while (next->level < i) {

		    right+=next->junction[i].step;
		    next=next->junction[i].n;

		}

		new->junction[i].step=right;
		prev->junction[i].step=left;

		sl->dirnode.junction[i].count++;

	    }

	    new->data=data;

	}

    }

    return new;

}

static void insert_sl_dirnode_left(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, void *found, unsigned int left, unsigned int right)
{
    struct sl_direntry_s *new=insert_sl_dirnode(sl, dirnode, data, left, right);

    if (new) move_sl_dirnode_left(sl, dirnode, left - right);

}

static void insert_sl_dirnode_right(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, void *found, unsigned int left, unsigned int right)
{
    struct sl_direntry_s *new=insert_sl_dirnode(sl, dirnode, data, left, right);

    if (new) move_sl_dirnode_right(sl, dirnode, right - left);

}

/*	insert an entry in the linked list and adapt the skiplist to that
	there are three situations:
	- entry is before the first dirnode
	- entry is after the last dirnode
	- every other case: entry is in between

	note: vector points to dirnode, which is the closest dirnode left to the entry
*/

static void sl_insert_cb(struct sl_skiplist_s *sl, struct sl_lock_ops *lockops, struct sl_vector_s *vector, struct sl_searchresult_s *result)
{

    if ((* lockops->upgrade_readlock_vector)(sl, vector, 0)==0) {
	struct sl_dirnode_s *dirnode=vector->path[vector->minlevel].dirnode;
	unsigned int left=result->step;
	unsigned int right=vector->path[vector->minlevel].step - left;
	void *data=NULL;

	if (result->flags & SL_SEARCHRESULT_FLAG_EXACT) {

	    return;

	} else if (result->flags & SL_SEARCHRESULT_FLAG_EMPTY) {

	    data=(* sl->ops.insert_after)(result->lookupdata, NULL, sl);
	    if (data==NULL) result->flags &= SL_SEARCHRESULT_FLAG_ERROR;
	    sl->dirnode.junction[0].step=1; /* one element */
	    return;

	} else if (result->flags & SL_SEARCHRESULT_FLAG_BEFORE) {

	    data=(* sl->ops.insert_before)(result->lookupdata, result->found, sl);
	    if (data==NULL) {

		result->flags &= SL_SEARCHRESULT_FLAG_ERROR;
		return;

	    }

	} else if (result->flags & SL_SEARCHRESULT_FLAG_AFTER) {

	    data=(* sl->ops.insert_after)(result->lookupdata, result->found, sl);
	    if (data==NULL) {

		result->flags &= SL_SEARCHRESULT_FLAG_ERROR;
		return;

	    }

	}

	if (left>=sl->prob && right>=sl->prob) {

	    /* put a dirnode on entry */

	    insert_sl_dirnode(sl, dirnode, data, left, right);

	} else if (left + right + 1 >= 2 * sl->prob) {

	    /* put a dirnode somewhere else */

	    if (left > right) {

		/* left of the new entry is more space */

		insert_sl_dirnode_left(sl, dirnode, data, left, right);

	    } else if (right > left) {

		/* right of the new entry is more space */

		insert_sl_dirnode_right(sl, dirnode, data, left, right);

	    }

	}

    }

}

void sl_insert(struct sl_skiplist_s *sl, struct sl_searchresult_s *result)
{
    struct sl_lockops_s *lockops=(result->flag & SL_SEARCHRESULT_FLAG_EXCLUSIVE) ? get_sl_lockops_nolock() : get_sl_lockops_default();

    sl_find_generic(sl, lockops, sl_insert_cb, result);
}
