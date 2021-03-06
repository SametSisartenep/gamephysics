#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <geometry.h>
#include "dat.h"
#include "fns.h"
#include "cmixer.h"

RFrame screenrf;
GameState state;
double t, Δt;

Sprite *ghoul, *ghoulbig;
Sprite *ness;


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
	ness->draw(ness, screen, toscreen(Pt2(-160,-120,1)));

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
rmbproc(void *arg)
{
	threadsetname("rmbproc");
	enum {
		RESET,
	};
	static char *items[] = {
	 [RESET]	"reset",
		nil
	};
	static Menu menu = { .item = items };
	Mousectl *mc;

	mc = (Mousectl*)arg;

	switch(menuhit(3, mc, &menu, _screen)){
	case RESET:
		resetsim();
		break;
	}

	threadexits(nil);
}

void
mouse(Mousectl *mc, Keyboardctl *)
{
	if((mc->buttons & 4) != 0)
		proccreate(rmbproc, mc, 2048);
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
soundproc(void *)
{
	threadsetname("soundproc");
	Biobuf *aout;
	uchar adata[512];

	aout = Bopen("/dev/audio", OWRITE);
	if(aout == nil)
		sysfatal("Bopen: %r");

	for(;;){
		cm_process((void *)adata, sizeof(adata)/2);
		Bwrite(aout, adata, sizeof adata);
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
	cm_Source *bgsound, *boatsound;

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
	cm_init(44100);
	cm_set_master_gain(0.5);

	display->locking = 1;
	unlockdisplay(display);

	screenrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);
	screenrf.bx = Vec2(1, 0);
	screenrf.by = Vec2(0,-1);

	ghoul = readsprite("assets/sheets/NpcCemet.pic", Pt(48,0), Rect(0,0,16,16), 5, 150);
	ghoulbig = newsprite(ghoul->sheet, Pt(144,64), Rect(0,0,24,24), 5, 120);
	ness = readsprite("assets/sheets/ness.pic", Pt(0,9), Rect(0,0,16,24), 20, 150);

	bgsound = cm_new_source_from_file("assets/sounds/birds.wav");
	if(bgsound == nil)
		sysfatal("cm_new_source_from_file: %s", cm_get_error());
	boatsound = cm_new_source_from_file("assets/sounds/steamboat.wav");
	if(boatsound == nil)
		sysfatal("cm_new_source_from_file: %s", cm_get_error());
	cm_set_loop(bgsound, 1);
	cm_set_loop(boatsound, 1);
	cm_set_gain(boatsound, 0.2);
	cm_play(bgsound);
	cm_play(boatsound);

	proccreate(soundproc, nil, 2048);

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
		ness->step(ness, frametime/1e6);

		redraw();

		sleep(66);
	}
}
