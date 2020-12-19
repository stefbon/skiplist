/*
  2010, 2011, 2012, 2013, 2014, 2015, 2016 Stef Bon <stefbon@gmail.com>

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

#ifndef _UTILS_SKIPLIST_H
#define _UTILS_SKIPLIST_H

#define _SKIPLIST_VERSION_MAJOR				1
#define _SKIPLIST_VERSION_MINOR				0
#define _SKIPLIST_VERSION_PATCH				0

#define _SKIPLIST_PROB					4
#define _SKIPLIST_MAXLANES				10

/* lock set on an entire dirnode when exact match is found by delete */
#define _DIRNODE_LOCK_RM				1

/* lock set in a lane/region to inserte/remove an entry */
#define _DIRNODE_LOCK_PREWRITE				2
#define _DIRNODE_LOCK_WRITE				4

/* lock set in a lane/region to read */
#define _DIRNODE_LOCK_READ				8

#define _SKIPLIST_READLOCK				1
#define _SKIPLIST_PREEXCLLOCK				2
#define _SKIPLIST_EXCLLOCK				3
#define _SKIPLIST_MUTEX					4

#define _SKIPLIST_FLAG_ALLOC				1

#define _DIRNODE_FLAG_START				1

struct sl_junction_s {
    unsigned char					index;
    unsigned  						step;
    struct sl_dirnode_s 				*n;
    struct sl_dirnode_s 				*p;
    unsigned int					count;
};

struct sl_dirnode_s {
    void 						*data;
    unsigned char					flags;
    unsigned char					lock;
    unsigned int					lockers;
    unsigned short					level;
    struct sl_junction_s				junction[];
};

struct sl_skiplist_s;

struct sl_ops_s {
    void 						*(* next) (void *data);
    void 						*(* prev) (void *data);
    int 						(* compare)(void *a, void *b);
    void 						*(* insert_before) (void *data, void *before, struct sl_skiplist_s *sl);
    void 						*(* insert_after) (void *data, void *after, struct sl_skiplist_s *sl);
    int 						(* delete) (void *data, struct sl_skiplist_s *sl);
    unsigned int 					(* count) (struct sl_skiplist_s *sl);
    void 						*(* first) (struct sl_skiplist_s *sl);
    void 						*(* last) (struct sl_skiplist_s *sl);
};

struct sl_skiplist_s {
    struct sl_ops_s					ops;
    unsigned 						prob;
    unsigned int					flags;
    pthread_mutex_t					mutex;
    pthread_cond_t					cond;
    unsigned short					maxlevel;
    struct sl_dirnode_s					dirnode[];
};

struct sl_path_s {
    unsigned char					index;
    struct sl_dirnode_s 				*dirnode;
    unsigned char					lockset;
    unsigned char					lock;
    unsigned int 					step;
};

struct sl_vector_s {
    unsigned short					maxlevel;
    unsigned short					minlevel;
    unsigned char					lockset;
    unsigned short					level;
    struct sl_path_s 					path[];
};

struct sl_lockops_s {
	void						(* remove_lock_vector)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, signed char change);
	void						(* readlock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *dirnode);
	void						(* remove_readlock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector);
	void						(* move_readlock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *next);
	int						(* upgrade_readlock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, unsigned char move);
	void						(* writelock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector, struct sl_dirnode_s *dirnode);
	void						(* remove_writelock_vector_path)(struct sl_skiplist_s *sl, struct sl_vector_s *vector);
};

#define SL_SEARCHRESULT_FLAG_FIRST				(1 << 0)
#define SL_SEARCHRESULT_FLAG_LAST				(1 << 1)
#define SL_SEARCHRESULT_FLAG_EXACT				(1 << 2)
#define SL_SEARCHRESULT_FLAG_BEFORE				(1 << 3)
#define SL_SEARCHRESULT_FLAG_AFTER				(1 << 4)
#define SL_SEARCHRESULT_FLAG_NOENT				(1 << 5)
#define SL_SEARCHRESULT_FLAG_DIRNODE				(1 << 6)
#define SL_SEARCHRESULT_FLAG_EMPTY				(1 << 7)
#define SL_SEARCHRESULT_FLAG_EXCLUSIVE				(1 << 8)
#define SL_SEARCHRESULT_FLAG_OK					(1 << 9)
#define SL_SEARCHRESULT_FLAG_ERROR				(1 << 20)

struct sl_searchresult_s {
    void						*lookupdata;
    void						*found;
    unsigned int					flags;
    unsigned int					row;
    unsigned int					step;
};

/* prototypes */

int init_sl_skiplist(struct sl_skiplist_s *sl,
		    void *(* next)(void *data), void *(*prev)(void *data),
		    int (* compare) (void *a, void *b),
		    void *(* insert_before) (void *data, void *before, struct sl_skiplist_s *sl),
		    void *(* insert_after) (void *data, void *after, struct sl_skiplist_s *sl),
		    void (* delete) (void *data, struct sl_skiplist_s *sl),
		    unsigned int (* count) (struct sl_skiplist_s *sl),
		    void *(* first) (struct sl_skiplist_s *sl),
		    void *(* last) (struct sl_skiplist_s *sl),
		    unsigned int *error);

struct sl_skiplist_s *create_sl_skiplist(unsigned char prob);

void clear_sl_skiplist(struct sl_skiplist_s *sl);
void free_sl_skiplist(struct sl_skiplist_s *sl);

void init_sl_searchresult(struct sl_searchresult_s *result, void *lookupdata, unsigned int flags);

#endif
