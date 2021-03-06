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
State state;
double timestep;


double min(double a, double b) { return a < b? a: b; }
double max(double a, double b) { return a > b? a: b; }

/*
 *	Dynamics stepper
 *
 * 	Currently set to a basic spring-damper system.
 */
double
accel(State *s, double t)
{
	static double k = 15, b = 0.1;

	USED(t);
	return -k*s->x - b*s->v;
}

Derivative
eval(State *s0, double t, double Δt, Derivative *d)
{
	State s;
	Derivative res;

	s.x = s0->x + d->dx*Δt;
	s.v = s0->v + d->dv*Δt;

	res.dx = s.v;
	res.dv = accel(&s, t+Δt);
	return res;
}

/*
 *	Explicit Euler Integrator
 */
void
euler0(State *s, double t, double Δt)
{
	static Derivative ZD = {0,0};
	Derivative d;

	d = eval(s, t, Δt, &ZD);

	s->x += d.dx*Δt;
	s->v += d.dv*Δt;
}

/*
 *	Semi-implicit Euler Integrator
 */
void
euler1(State *s, double t, double Δt)
{
	static Derivative ZD = {0,0};
	Derivative d;

	d = eval(s, t, Δt, &ZD);

	s->v += d.dv*Δt;
	s->x += s->v*Δt;
}

/*
 *	RK4 Integrator
 */
void
rk4(State *s, double t, double Δt)
{
	static Derivative ZD = {0,0};
	Derivative a, b, c, d;
	double dxdt, dvdt;

	a = eval(s, t, 0, &ZD);
	b = eval(s, t, Δt/2, &a);
	c = eval(s, t, Δt/2, &b);
	d = eval(s, t, Δt, &c);

	dxdt = 1.0/6 * (a.dx + 2*(b.dx + c.dx) + d.dx);
	dvdt = 1.0/6 * (a.dv + 2*(b.dv + c.dv) + d.dv);

	s->x += dxdt*Δt;
	s->v += dvdt*Δt;
}

/*
 *	The Integrator
 */
void
integrate(State *s, double t, double Δt)
{
	//euler0(s, t, Δt);
	//euler1(s, t, Δt);
	rk4(s, t, Δt);
}

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
	} l = {
		Pt2(-2,v,1),
		Pt2(2,v,1)
	};
	char buf[32];

	line(screen, toscreen(l.p0), toscreen(l.p1), 0, 0, 0, display->white, ZP);
	snprint(buf, sizeof buf, "%.3f", v);
	l.p1.x += 10;
	string(screen, toscreen(l.p1), display->white, ZP, font, buf);
}

void
redraw(void)
{
	lockdisplay(display);

	draw(screen, screen->r, display->black, nil, ZP);
	drawtimestep(timestep);
	drawbar(state.min); drawbar(state.max); drawbar(state.avg);
	fillellipse(screen, toscreen(Pt2(0,state.x,1)), 2, 2, display->white, ZP);

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
		//resetsim();
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
	double t, Δt;

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
	screenrf.p = Pt2(screen->r.min.x+Dx(screen->r)/2,screen->r.max.y-Dy(screen->r)/2,1);
	screenrf.bx = Vec2(1, 0);
	screenrf.by = Vec2(0,-1);

	t = 0;
	Δt = 0.01;
	state.x = 100;
	state.v = 0;

	display->locking = 1;
	unlockdisplay(display);
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

		integrate(&state, t, Δt);

		state.acc += state.x;
		state.avg = state.acc/++state.nsteps;
		state.min = min(state.min, state.x);
		state.max = max(state.max, state.x);

		redraw();

		t += Δt;
		timestep = t;

		sleep(66);
	}
}
