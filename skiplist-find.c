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
#include "skiplist-find.h"
#include "logging.h"

void sl_find_generic(struct sl_skiplist_s *sl, struct sl_lockops_s *lockops, void *cb(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_searchresult_s *result), struct sl_searchresult_s *result)
{
    struct sl_dirnode_s *dirnode=sl->dirnode;
    unsigned short level=sl->level;
    void *next=NULL;
    struct sl_vector_s *vector=NULL;
    void *search=NULL;
    int diff=0;

    if (sl->ops.count(sl)==0) {

	result->flags |= SL_SEARCHRESULT_FLAG_EMPTY;
	return;

    }

    if ((vector=create_vector_path(sl->dirnode.level+1))==NULL) {;

	result->flags |= SL_SEARCHRESULT_FLAG_ERROR;
	return;

    }

    /* create the locking startpoint at the left top of the skiplist */
    (* lockops->readlock_vector_path)(sl, vector, sl->dirnode.level+1, dirnode);
    vector->level--;

    /* first check the extreme cases: before the first or the last

	TODO: do something with the information the name to lookup
	is closer to the last than the first */

    search=sl->ops.last(sl);
    diff=sl->ops.compare(search, result->lookupdata);

    if (diff<0) {

	/* after the last: not found */

	result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_LAST | SL_SEARCHRESULT_FLAG_AFTER);
	result->found=search;
	result->row=sl->ops.count(sl);
	goto out;

    }

    search=sl->ops.first(sl);
    result->row=1;
    diff=sl->ops.compare(search, result->lookupdata);

    if (diff>0) {

	/* before the first: not found */

	result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_FIRST | SL_SEARCHRESULT_FLAG_BEFORE);
	result->found=search;
	result->row=0;
	goto out;

    }

    (* lockops->readlock_vector_path)(sl, vector, dirnode);

    while (vector->level>=0) {
	struct sl_dirnode_s *next_dirnode=dirnode->junction[vector->level].next;

	if (next_dirnode) {

	    next=next_dirnode->data;
	    diff=sl->ops.compare(next, result->lookupdata);

	} else {

	    diff=1;

	}

	if (diff>0) {

	    /* diff>0: this next entry is too far, go one level down
		set another readlock on the same dirnode/junction */

	    if (vector->level==0) break;
	    vector->level--;

	} else if (diff==0) {

	    /* exact match */

	    result->found=next;
	    result->row+=dirnode->junction[vector->level].count - 1;
	    (* lockops->move_readlock_vector_path)(sl, vector, next_dirnode);
	    result->flags |= (SL_SEARCHRESULT_FLAG_EXACT | SL_SEARCHRESULT_FLAG_DIRNODE);
	    for (unsigned int i=0; i<vector->level; i++) vector->path[i].dirnode=dirnode;
	    vector->level=0;
	    goto out;

	} else {

	    /* res<0: next_entry is smaller than name: skip
		move lock from previous, the level/lane stays the same  */

	    result->row+=dirnode->junction[vector->level].count;
	    (* lockops->move_readlock_vector_path)(sl, vector, next_dirnode);
	    dirnode=next_dirnode;
	    search=sl->ops.next(next); /* next is not the entry looking for: take the first following */

	    if (search==NULL) {

		/* there is no next entry */

		result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_LAST | SL_SEARCHRESULT_FLAG_AFTER);
		result->found=next;
		for (unsigned int i=0; i<vector->level; i++) vector->path[i].dirnode=dirnode;
		goto out;

	    }

	    result->step=1;

	}

    }

    while (search) {

	diff=sl->ops.compare(search, result->lookupdata);

	if (diff<0) {

	    /* before name still */
	    search=sl->ops.next(search);
	    result->row++;
	    result->step++;

	} else if (diff==0) {

	    /* exact match */
	    result->found=search;
	    result->flags |= SL_SEARCHRESULT_FLAG_EXACT;
	    break;

	} else {

	    /* past name, no exact match */
	    result->found=search;
	    result->flags |= (SL_SEARCHRESULT_FLAG_NOENT | SL_SEARCHRESULT_FLAG_BEFORE);
	    break;

	}

    }

    out:

    (* cb)(sl, lockops, vector, result);
    (* lockops->remove_lock_vector)(sl, vector);

}

static void sl_find_cb(struct sl_skiplist_s *sl, struct sl_lock_ops *l, struct sl_vector_s *v, struct sl_searchresult_s *r)
{
    /* when finding an entry do nothing ... */
}

void *sl_find(struct sl_skiplist_s *sl, struct sl_searchresult_s *result))
{
    struct sl_lockops_s *lockops=(result->flags & SL_SEARCHRESULT_FLAG_EXCLUSIVE) ? get_sl_lockops_nolock() : get_sl_lockops_default();

     sl_find_generic(sl, lockops, sl_find_cb, result);
}
