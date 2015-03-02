#include "GroundPlaneCalibration.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "GroundCalibrationConst.h"

GroundPlaneCalibration::GroundPlaneCalibration()
{
	m_bCalibed = false;
	m_gpRecog = new GPGesutureRecognition;
}


GroundPlaneCalibration::~GroundPlaneCalibration()
{
}

bool GroundPlaneCalibration::LoadCalibFile(const char* filename){
	FILE* fp;
	char line[4][128];

	fopen_s(&fp, filename, "r");
	if (fp == NULL){
		return false;
	}

	if (fgets(line[0], 128, fp) == NULL) return false;
	if (fgets(line[1], 128, fp) == NULL) return false;
	if (fgets(line[2], 128, fp) == NULL) return false;
	if (fgets(line[3], 128, fp) == NULL) return false;

	a = atof(line[0]);
	b = atof(line[1]);
	c = atof(line[2]);
	d = atof(line[3]);

	// Pre caliculation 
	sqabc = sqrt(a*a + b*b + c*c);

	printf("a:%f, b:%f, c:%f, d:%f\n", a, b, c, d);


	char origin_s[128];
	if (fgets(origin_s, 128, fp) == NULL) return false;
	sscanf_s(origin_s, "%f %f %f", &origin[0], &origin[1], &origin[2]);
	printf("%f %f %f\n", origin[0], origin[1], origin[2]);

	char unitZ_s[128];
	if (fgets(unitZ_s, 128, fp) == NULL) return false;
	sscanf_s(unitZ_s, "%f %f %f", &unitZ[0], &unitZ[1], &unitZ[2]);
	printf("%f %f %f\n", unitZ[0], unitZ[1], unitZ[2]);
	
	char unitX_s[128];
	if (fgets(unitX_s, 128, fp) == NULL) return false;
	sscanf_s(unitX_s, "%f %f %f", &unitX[0], &unitX[1], &unitX[2]);
	printf("%f %f %f\n", unitX[0], unitX[1], unitX[2]);

	fclose(fp);

	float length = pow(a * a + b * b + c * c, 0.5f);// 1.0 / d;
	unitY[0] = (a / length) / 100.0;
	unitY[1] = (b / length) / 100.0;
	unitY[2] = (c / length) / 100.0;
	printf("%f %f %f\n", unitY[0], unitY[1], unitY[2]);

	m_bCalibed = true;

	return true;
}
bool GroundPlaneCalibration::WriteCalibFile(const char* filename){
	FILE* fp;
	char line[4][128];

	fopen_s(&fp, filename, "w");
	if (fp == NULL){
		return false;
	}

	sprintf_s(line[0], "%f\n", a);
	sprintf_s(line[1], "%f\n", b);
	sprintf_s(line[2], "%f\n", c);
	sprintf_s(line[3], "%f\n", d);

	fputs(line[0], fp);
	fputs(line[1], fp);
	fputs(line[2], fp);
	fputs(line[3], fp);

	char origin_s[128];
	sprintf_s(origin_s, "%f %f %f\n", origin[0], origin[1], origin[2]);
	fputs(origin_s, fp);
	

	char unitZ_s[128];
	sprintf_s(unitZ_s, "%f %f %f\n", unitZ[0], unitZ[1], unitZ[2]);
	fputs(unitZ_s, fp);

	char unitX_s[128];
	sprintf_s(unitX_s, "%f %f %f\n", unitX[0], unitX[1], unitX[2]);
	fputs(unitX_s, fp);

	fclose(fp);

	return true;
}

void GroundPlaneCalibration::CalcGroundPlane(float p0[3], float p1[3], float p2[3]){
	a = (p1[1] - p0[1])*(p2[2] - p0[2]) - (p2[1] - p0[1])*(p1[2] - p0[2]);
	b = (p1[2] - p0[2])*(p2[0] - p0[0]) - (p2[2] - p0[2])*(p1[0] - p0[0]);
	c = (p1[0] - p0[0])*(p2[1] - p0[1]) - (p2[0] - p0[0])*(p1[1] - p0[1]);
	d = -(a*p0[0] + b*p0[1] + c*p0[2]);
	// Pre caliculation 
	sqabc = sqrt(a*a+b*b+c*c);
	
	
	origin[0] = p0[0], origin[1] = p0[1], origin[2] = p0[2];
	
	{
		float vec[3];
		vec[0] = p1[0] - p0[0];
		vec[1] = p1[1] - p0[1];
		vec[2] = p1[2] - p0[2];
		
		//float length = pow(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2], 0.5f);
		//unitZ[0] = vec[0] / length;
		//unitZ[1] = vec[1] / length;
		//unitZ[2] = vec[2] / length;

		unitZ[0] = vec[0] / GROUNDCALIBRATION_LENDTH;
		unitZ[1] = vec[1] / GROUNDCALIBRATION_LENDTH;
		unitZ[2] = vec[2] / GROUNDCALIBRATION_LENDTH;

	}

	{
		float vec[3];
		vec[0] = p2[0] - p0[0];
		vec[1] = p2[1] - p0[1];
		vec[2] = p2[2] - p0[2];

		//float length = pow(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2], 0.5f);
		//unitX[0] = vec[0] / length;
		//unitX[1] = vec[1] / length;
		//unitX[2] = vec[2] / length;
		unitX[0] = vec[0] / GROUNDCALIBRATION_LENDTH;
		unitX[1] = vec[1] / GROUNDCALIBRATION_LENDTH;
		unitX[2] = vec[2] / GROUNDCALIBRATION_LENDTH;
	}

	float length = pow(a * a + b * b + c * c, 0.5f);// 1.0 / d;
	unitY[0] = (a /length) / 100.0;
	unitY[1] = (b / length) / 100.0;
	unitY[2] = (c / length) / 100.0;


	printf("a:%f, b:%f, c:%f, d:%f\n", a, b, c, d);
	printf("%f %f %f\n", origin[0], origin[1], origin[2]);
	printf("%f %f %f\n", unitZ[0], unitZ[1], unitZ[2]);
	printf("%f %f %f\n", unitX[0], unitX[1], unitX[2]);
	printf("%f %f %f\n", unitY[0], unitY[1], unitY[2]);

	m_bCalibed = true;
}

float GroundPlaneCalibration::CalcDistanceToPlane(float p[3]){
	return abs(a*p[0] + b*p[1] + c*p[2] + d) / sqabc;
}

void GroundPlaneCalibration::CalcCalibedCoordinates(float z, float x, float *ox, float *oy, float *oz){
	z *= 100.0;
	x *= 100.0;
	*ox = origin[0] + unitZ[0] * z + unitX[0] * x;
	*oy = origin[1] + unitZ[1] * z + unitX[1] * x;
	*oz = origin[2] + unitZ[2] * z + unitX[2] * x;
}

void GroundPlaneCalibration::CalcCalibedCoordinates3D(float x, float y, float z, float *ox, float *oy, float *oz){
	z *= 100.0;
	x *= 100.0;
	y *= 100.0;
	*ox = origin[0] + unitZ[0] * z + unitX[0] * x + unitY[0] * y;
	*oy = origin[1] + unitZ[1] * z + unitX[1] * x + unitY[1] * y;
	*oz = origin[2] + unitZ[2] * z + unitX[2] * x + unitY[2] * y;
}

void GroundPlaneCalibration::CalcMove3D(float x, float y, float z, float *ox, float *oy, float *oz){
	z *= 100.0;
	x *= 100.0;
	y *= 100.0;
	*ox = unitZ[0] * z + unitX[0] * x + unitY[0] * y;
	*oy = unitZ[1] * z + unitX[1] * x + unitY[1] * y;
	*oz = unitZ[2] * z + unitX[2] * x + unitY[2] * y;
}