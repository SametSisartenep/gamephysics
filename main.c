#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"

RFrame screenrf;
GameState state;
double t, Δt;

Sprite *ghoul, *ghoulbig;


Point
toscreen(Point2 p)
{
	p = invrframexform(p, screenrf);
	return Pt(p.x,p.y);
}

Point2
fromscreen(Point p)
{
	return rframexform(Pt2(p.x,p.y,1), screenrf);
}

//void
//drawsimstats(void)
//{
//	char buf[128];
//	int i;
//
//	for(i = 0; i < NSTATS; i++){
//		snprint(buf, sizeof buf, "%.3f/%.3f/%.3f[%.3f]",
//			simstats[i].min, simstats[i].avg, simstats[i].max,
//			simstats[i].cur);
//		string(screen, addpt(screen->r.min, Pt(10,30+i*font->height)), display->white, ZP, font, buf);
//	}
//}

void
drawtimestep(double t)
{
	char buf[32];

	snprint(buf, sizeof buf, "t=%gs", t);
	string(screen, addpt(screen->r.min, Pt(10,10)), display->white, ZP, font, buf);
}

void
drawbar(double v)
{
	struct {
		Point2 p0, p1;
	} bar = {
		Pt2(-2,v,1),
		Pt2(2,v,1)
	};
	char buf[32];

	line(screen, toscreen(bar.p0), toscreen(bar.p1), 0, 0, 0, display->white, ZP);
	snprint(buf, sizeof buf, "%.3f", v);
	bar.p1.x += 10;
	string(screen, toscreen(bar.p1), display->white, ZP, font, buf);
}

void
redraw(void)
{
	lockdisplay(display);

	draw(screen, screen->r, display->black, nil, ZP);
	drawtimestep(t);
	drawbar(state.stats.min); drawbar(state.stats.max); drawbar(state.stats.avg);
	fillellipse(screen, toscreen(Pt2(0,state.x,1)), 2, 2, display->white, ZP);

	ghoul->draw(ghoul, screen, toscreen(Pt2(100,-100,1)));
	ghoulbig->draw(ghoulbig, screen, toscreen(Pt2(-100,-100,1)));

	flushimage(display, 1);
	unlockdisplay(display);
}

void
resized(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		sysfatal("resize failed");
	unlockdisplay(display);
	screenrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);
	redraw();
}

void
resetsim(void)
{
	memset(&state, 0, sizeof(GameState));
	state.x = 100;
	state.stats.update = statsupdate;
	t = 0;
}

void
rmb(Mousectl *mc)
{
	enum {
		RESET,
	};
	static char *items[] = {
	 [RESET]	"reset",
		nil
	};
	static Menu menu = { .item = items };

	switch(menuhit(3, mc, &menu, _screen)){
	case RESET:
		resetsim();
		break;
	}
}

void
mouse(Mousectl *mc, Keyboardctl *)
{
	if((mc->buttons & 4) != 0)
		rmb(mc);
}

void
key(Rune r)
{
	switch(r){
	case Kdel:
	case 'q':
		threadexitsall(nil);
	}
}

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mousectl *mc;
	Keyboardctl *kc;
	Rune r;
	uvlong then, now;
	double frametime, timeacc;

	ARGBEGIN{
	default: usage();
	}ARGEND;
	if(argc > 0)
		usage();

	if(initdraw(nil, nil, nil) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	display->locking = 1;
	unlockdisplay(display);

	screenrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);
	screenrf.bx = Vec2(1, 0);
	screenrf.by = Vec2(0,-1);

	ghoul = readsprite("assets/sheets/NpcCemet.pic", Pt(48,0), Rect(0,0,16,16), 5, 150);
	ghoulbig = newsprite(ghoul->sheet, Pt(144,64), Rect(0,0,24,24), 5, 120);

	Δt = 0.01;
	then = nanosec();
	timeacc = 0;
	resetsim();

	redraw();

	for(;;){
		enum { MOUSE, RESIZE, KEYBOARD };
		Alt a[] = {
			{mc->c, &mc->Mouse, CHANRCV},
			{mc->resizec, nil, CHANRCV},
			{kc->c, &r, CHANRCV},
			{nil, nil, CHANNOBLK}
		};

		switch(alt(a)){
		case MOUSE:
			mouse(mc, kc);
			break;
		case RESIZE:
			resized();
			break;
		case KEYBOARD:
			key(r);
			break;
		}

		now = nanosec();
		frametime = now - then;
		then = now;
		timeacc += frametime/1e9;

		while(timeacc >= Δt){
			integrate(&state, t, Δt);

			state.stats.update(&state.stats, state.x);

			timeacc -= Δt;
			t += Δt;
		}

		ghoul->step(ghoul, frametime/1e6);
		ghoulbig->step(ghoulbig, frametime/1e6);

		redraw();

		sleep(66);
	}
}
