#include "Field.h"

//------------------------------------------
// Constructor & Destructor
//------------------------------------------

CField::CField()
{
	_tile = new std::list<SessionID>(TILE_Y * TILE_X);


}
CField::~CField()
{

}


