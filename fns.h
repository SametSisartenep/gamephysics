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

/*
 *	physics
 */
void integrate(GameState*, double, double);

/*
 *	sprite
 */
Sprite *newsprite(Image*, Point, Rectangle, int, ulong);
Sprite *readsprite(char*, Point, Rectangle, int, ulong);
void delsprite(Sprite*);
