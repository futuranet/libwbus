/*
 * GSM control module
 */

#include "machine.h"

typedef struct gsmctl *HANDLE_GSMCTL;

typedef void (*gsmcallback)(HANDLE_GSMCTL hgsmctl, void *data);

/*
 * Get GSM status as text
 */
void gsmctl_getStatus(HANDLE_GSMCTL hgsmctl, char *stext);

/*
 * Get time from GSM network
 * \return 0 on success, nonzero means failure. 
 */
int gsmctl_getTime(HANDLE_GSMCTL hgsmctl, rtc_time_t *time);

/*
 * Get one of the allowd phone numbers by index (start at 0).
 * \return 0 if idx is a valid index or non-zero if index is not used/valid
 */
int gsmctl_getNumber(HANDLE_GSMCTL hgsmctl, char *out, int idx);


/*
 * Remove one of the allowd phone numbers by index (start at 0).
 */
void gsmctl_removeNumber(HANDLE_GSMCTL hgsmctl, int idx);

/*
 * Add next incoming calling phon number to allowed list.
 */
void gsmctl_addNumber(HANDLE_GSMCTL hgsmctl);

/*
 * Get last received not yet added number
 */
int gsmctl_getNewNumber(HANDLE_GSMCTL hgsmctl, char *out);

/*
 * Save current allowed numbers into non-volatile memory
 */
void gsmctl_saveNumbers(HANDLE_GSMCTL hgsmctl);

/*
 * Register a callback function for successful incoming requests (calls
 * initiated from a phone number that are allowed).
 */
void gsmctl_registerCallback(HANDLE_GSMCTL hgsmctl, gsmcallback cb, void *data);

/*
 * Pass main thread control to GSMCTL for doing I/O
 */
void gsmctl_thread(HANDLE_GSMCTL hgsmctl);

/*
 * Open an gsm ctl instance.
 */
int gsmctl_open(HANDLE_GSMCTL *phgsmctl, int dev);

/*
 * Cancel add number operation
 */
void gsmctl_addNumberCancel(HANDLE_GSMCTL hgsmctl);

/*
 * Close a gsm ctl instance
 */
void gsmctl_close(HANDLE_GSMCTL hgsmctl);

