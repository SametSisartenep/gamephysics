typedef struct State State;
typedef struct Derivative Derivative;

struct State
{
	double x, v;
	double acc, min, max, avg;
	int nsteps;
};

struct Derivative
{
	double dx, dv;
};
