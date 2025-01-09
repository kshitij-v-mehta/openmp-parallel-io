/*
 * stripe_handler.h
 *
 *  Created on: May 4, 2010
 *      Author: kmehta
 */

#ifndef STRIPE_HANDLER_H_
#define STRIPE_HANDLER_H_

int stripe_handler(FS *fs, void *bufptr, long buflen, off_t offset);

#endif /* STRIPE_HANDLER_H_ */
