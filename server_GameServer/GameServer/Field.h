#ifndef __FILED_H__
#define __FILED_H__

#include <list>

#include "Contents.h"

using SessionID = uint64_t;
class CField
{
public:
	//------------------------------------------
	// Constructor & Destructor
	//------------------------------------------

	CField();
	~CField();

private:
	std::list<SessionID>* _tile;
};

#endif