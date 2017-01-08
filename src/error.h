/*
 * $Id: error.h,v 1.1 2000/03/29 01:20:07 shane Exp $
 */

#ifndef ERROR_H
#define ERROR_H

typedef struct {
    int error_code;
    char desc[128];
} error_t;

/* methods */
void error_init(error_t *err, int error_code, const char *desc_fmt, ...);
int error_get_error_code(const error_t *err);
const char *error_get_desc(const error_t *err);

#endif /* ERROR_H */
