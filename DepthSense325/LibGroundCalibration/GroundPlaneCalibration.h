#pragma once
#include "GPGesutureRecognition.h"

class GroundPlaneCalibration
{
protected:
	float a, b, c, d;
	float sqabc;
	float origin[3];
	float unitX[3], unitZ[3], unitY[3];
	
	bool m_bCalibed;

	GPGesutureRecognition* m_gpRecog;
public:
	/*
	---file format---
	a
	b
	c
	d
	EOF
	*/
	bool LoadCalibFile(const char* filename);
	bool WriteCalibFile(const char* filename);

	bool IsCalibed() { return m_bCalibed; };

	void CalcGroundPlane(float p0[3], float p1[3], float p2[3]);
	float CalcDistanceToPlane(float p[3]);

	// meter coordination. 1.0 = 1m 0.01 = 1cm.
	void CalcCalibedCoordinates(float z, float x, float *ox, float *oy, float *oz);
	void CalcCalibedCoordinates3D(float x, float y, float z, float *ox, float *oy, float *oz);

	void CalcMove3D(float x, float y, float z, float *ox, float *oy, float *oz);

	GPGesutureRecognition* GetRecog() { return m_gpRecog; }; 

public:
	GroundPlaneCalibration();
	virtual ~GroundPlaneCalibration();
};

