//This is basically a helper file that has general-usage behavior interfaces for rendering
#ifndef _PHYSICS_INTERFACES_H_
#define _PHYSICS_INTERFACES_H_

class VelocityInterface
{
public:
	virtual VectorF getVelocity()=0;
};

#endif