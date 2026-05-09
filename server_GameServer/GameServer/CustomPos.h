#ifndef __CUSTOM_POS_H__
#define __CUSTOM_POS_H__
#include <stdint.h>
#include <algorithm>

#include "Contents.h"

#define FLOAT_CLAMP(val, min, max)	((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val))

constexpr const double PI = 3.1415926535;
inline double ThetaToRadian(uint16_t theta)
{
	return static_cast<double>(theta) * PI / 180.0;
}
inline uint16_t RadianToTheta(double radian)
{
	return static_cast<uint16_t>(radian * 180.0 / PI);
}
struct CustomPos
{
	CustomPos(float y, float x, uint16_t r) noexcept
		:y(y), x(x), r(r) { }
	CustomPos(const CustomPos&) noexcept = default;
	CustomPos& operator=(const CustomPos&) noexcept = default;
	CustomPos& SetPos(float _y, float _x, uint16_t _r) noexcept { y = _y; x = _x; r = _r; }

	
	// 이동되었으면 true, 그대로이면 false
	bool Move(double moveDist) noexcept
	{
		float newy = y - static_cast<float>(cos(ThetaToRadian(r)) * moveDist);
		float newx = x - static_cast<float>(sin(ThetaToRadian(r)) * moveDist);
		float befy = y; 
		float befx = x;

		y = FLOAT_CLAMP(newy, Map_MinY(), Map_MaxY());
		x = FLOAT_CLAMP(newx, Map_MinX(), Map_MaxX());

		return (std::abs(befy - y) > 0.001f || std::abs(befx == x) > 0.001f);
	}

	// 실제로 회전되었으면 true, 그대로이면 false
	bool Rotate(const CustomPos& dest) noexcept
	{
		uint16_t befr = r;
		
		double directionY = static_cast<double>(dest.y - y);
		double directionX = static_cast<double>(dest.x - x);
		if (directionY < 0.001 || directionX < 0.001) 
			return false;

		r = RadianToTheta(std::atan2(directionY, directionX));
		if (r < 0) 
			r += 360;

		return (r != befr);
	}

	

	float y;
	float x;
	uint16_t r;
};



#endif
