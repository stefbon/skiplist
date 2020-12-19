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

#ifndef ENOATTR
#define ENOATTR ENODATA        /* No such attribute */
#endif

#include "skiplist.h"
#include "skiplist-find.h"
#include "skiplist-delete.h"
#include "skiplist-utils.h"
#define LOGGING
#include "logging.h"

#define _DIRNODE_ACTION_REMOVE					1
#define _DIRNODE_ACTION_MOVELEFT				2
#define _DIRNODE_ACTION_MOVERIGHT				3

static unsigned char correct_sl_dirnodes(struct sl_skiplist_s *sl, struct sl_dirnode_s *dirnode, unsigned int left, unsigned int right, unsigned char remove, unsigned int *p_steps)
{
    unsigned char action=0;

    if (left + right + 1 < 2 * sl->prob) {

	/* remove dirnode */

	remove_sl_dirnode(sl, dirnode);
	action=_DIRNODE_ACTION_REMOVE;

	/* here free the dirnode: make sure it's totally free */

    } else {

	/* move dirnode to prevent to much space left or right */

	if (left>right+1) {

	    /* more space at left side */

	    move_sl_dirnode_left(sl, dirnode, left-right, p_steps);
	    action=_DIRNODE_ACTION_MOVELEFT;

	} else if (right>left+1) {

	    move_sl_dirnode_right(sl, dirnode, right-left, p_steps);
	    action=_DIRNODE_ACTION_MOVERIGHT;

	} else if (remove) {

	    remove_sl_dirnode(sl, dirnode);
	    action=_DIRNODE_ACTION_REMOVE;

	}

    }

    return action;

}


static void sl_delete_cb(struct sl_skiplist_s *sl, struct sl_lock_ops *lockops, struct sl_vector_s *vector, struct sl_searchresult_s *result)
{

    if (result->flags & SL_SEARCHRESULT_FLAG_EXACT) {
	unsigned char move=(result->flags & SL_SEARCHRESULT_FLAG_DIRNODE) : 1 : 0;
	unsigned int size=sizeof(struct sl_vector_s) + vector->maxlevel * sizeof(struct sl_path_s);
	char buffer[size];
	struct sl_vector_s *orig=(struct sl_vector_s *) buffer;

	memcpy(orig, vector, size);

	/* upgrade to write lock */

	if ((*lockops->upgrade_readlock_vector)(sl, vector, move)==0) {
	    struct sl_dirnode_s *dirnode=orig->path[vector->level].dirnode;
	    unsigned int right=dirnode->junction[0].step;
	    signed char action=0;
	    unsigned int steps=0;

	    if (result->flags & SL_SEARCHRESULT_FLAG_DIRNODE) {
		struct sl_dirnode_s *prev=dirnode->junction[0].p;
		unsigned int left=prev->junction[0].step;

		action=correct_sl_dirnodes(sl, dirnode, left, right, 1, &steps);
		if (action==_DIRNODE_ACTION_MOVERIGHT || action==_DIRNODE_ACTION_REMOVE) dirnode=prev;

	    } else {

		if (right < sl->prob - 1) {

		    /* getting too close: check where is space */

		    if (dirnode->flags & _DIRNODE_FLAG_START) {
			struct sl_dirnode_s *next=dirnode->junction[0].n;

			steps=right - result->steps;

			if ((next->flags & _DIRNODE_FLAG_START)==0) {
			    unsigned int right2=next->junction[0].step;
			    unsigned char tmp=correct_sl_dirnodes(sl, next, right, right2, 0, &steps);

			    if (tmp==_DIRNODE_ACTION_MOVELEFT && (right - steps <= result->steps)) {

				dirnode=next;
				action=_DIRNODE_ACTION_MOVELEFT;

			    }

			} /* else ... ??? */

		    } else {
			struct sl_dirnode_s *prev=dirnode->junction[0].p;
			unsigned int left=prev->junction[0].step;

			steps=result->steps;
			action=correct_sl_dirnodes(sl, dirnode, left, right, 0, &steps);

			if (action==_DIRNODE_ACTION_MOVERIGHT && steps >= result->steps) {

			    dirnode=prev;
			    action=_DIRNODE_ACTION_MOVELEFT;

			}

		    }

		}

	    }

	    if (sl->ops.delete(result->found, sl)==0) {

		if (action==_DIRNODE_ACTION_MOVELEFT || action==_DIRNODE_ACTION_MOVERIGHT || action==_DIRNODE_ACTION_REMOVE) {
		    unsigned short ctr=0;

		    while (ctr < _SKIPLIST_MAXLANES) {

			/* all those lanes have lost one entry */

			for (unsigned int i=ctr; i<dirnode->level; i++, ctr++) dirnode->junction[].step--;
			if (ctr < _SKIPLIST_MAXLANES && (dirnode->flags & _DIRNODE_FLAG_START)==0) {

			    dirnode=dirnode->junction[dirnode->level - 1].p;

			}

		    }

		}

	    }

	    /* set a flag the operation is successfull */

	} else {

	    logoutput("sl_delete_cb: not able to update readlock");
	    result->flags |= SL_SEARCH_FLAG_ERROR;

	}

    }

}

void sl_delete(struct sl_skiplist_s *sl, struct sl_searchresult_s *result)
{
    struct sl_lock_ops *lockops=(result->flag & SL_SEARCHRESULT_FLAG_EXCLUSIVE) ? get_sl_lockops_nolock() : get_sl_lockops_default();
    sl_find_generic(sl, lockops, sl_delete_cb, result);
}
