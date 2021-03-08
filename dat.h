typedef struct State State;
typedef struct Derivative Derivative;
typedef struct Stats Stats;

struct Stats
{
	double cur;
	double total;
	double min, avg, max;
	uvlong nupdates;

	void (*update)(Stats*, double);
};

struct State
{
	double x, v;
	Stats stats;
};

struct Derivative
{
	double dx, dv;
};
