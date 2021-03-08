/*
 *	alloc
 */
void *emalloc(ulong);
void *erealloc(void*, ulong);
Image *eallocimage(Display*, Rectangle, ulong, int, ulong);

/*
 *	stats
 */
void statsupdate(Stats*, double);
