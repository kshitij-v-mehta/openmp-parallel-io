/*
 * list_utils.h
 *
 *  Created on: May 5, 2010
 *      Author: kmehta
 */

#ifndef LIST_UTILS_H_
#define LIST_UTILS_H_

int merge_args_lists(FS *fs);
int add_to_assignment_list(FS *fs, void *_bufptr, long _buflen, off_t _offset);
int delete_top_node(FS *fs);

#endif /* LIST_UTILS_H_ */
