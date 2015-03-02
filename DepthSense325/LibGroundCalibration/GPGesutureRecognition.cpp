#include "GPGesutureRecognition.h"

#include <stdio.h>

#include "GroundCalibrationConst.h"

GPGesutureRecognition::GPGesutureRecognition()
{
	m_bBasicPose = true;
	m_currentPose = m_previosPose = GPGESTURE_BASIC;
	m_head = m_footLeft = m_footRight = 0.0;
	m_nDownCount = m_nHandCount = m_nJumpCount = m_nWalkCount = 0;

	m_handState = 0;

	m_footThreshold = GROUNDCALIBRATION_THRESHOLD_FOOT;
	m_headThreshold = GROUNDCALIBRATION_THRESHOLD_HEAD;
}


GPGesutureRecognition::~GPGesutureRecognition()
{
}

GPGestureState GPGesutureRecognition::RecognizeGesture(float head, float footL, float footR, float handR, float handL){
	GPGestureState gps;
	int gpe = 0;
	m_head = head;
	m_footLeft = footL;
	m_footRight = footR;

	if (m_previosPose == GPGESTURE_BASIC){
		gpe = FromBasic();
	}else{
		//printf("non basic\n");
		if (m_previosPose & GPGESTURE_WALK){
			gpe |= FromWalk();
		}
		if (m_previosPose & GPGESTURE_JUMP){
			gpe |= FromJump();
		}
		if (m_previosPose & GPGESTURE_DOWN){
			gpe |= FromDown();
		}
		if (m_previosPose & GPGESTURE_HAND){
			gpe |= FromHand();
		}
	}

	int HState = 0;
	HState += handR > GROUNDCALIBRATION_THRESHOLD_HEAD ? 1 : 0;
	HState += handL > GROUNDCALIBRATION_THRESHOLD_HEAD ? 2 : 0;
	m_handState = HState;

	//printf("hand %d\n", HState);

	gps.Set(gpe, gpe != m_previosPose);
	m_previosPose = gpe;
	m_currentPose = gpe;
	return gps;
}

int GPGesutureRecognition::FromBasic(){
	int gpe = GPGESTURE_BASIC;
	if (m_footLeft > m_footThreshold && m_footRight > m_footThreshold){
		m_nJumpCount++;
		gpe = GPGESTURE_JUMP;
	}
	else if (m_footLeft > m_footThreshold || m_footRight > m_footThreshold){
		m_nWalkCount++;
		gpe = GPGESTURE_WALK;
	}
	else if (m_head < m_headThreshold){
		m_nDownCount++;
		gpe = GPGESTURE_DOWN;
	}
	return gpe;
}

int GPGesutureRecognition::FromWalk(){
	if (m_footLeft > m_footThreshold && m_footRight > m_footThreshold){
		m_nJumpCount++;
		return GPGESTURE_JUMP;
	}
	else if (m_footLeft < m_footThreshold && m_footRight < m_footThreshold){
		
		return GPGESTURE_BASIC;
	}
	
	return GPGESTURE_WALK;
}

int GPGesutureRecognition::FromJump(){
	if (m_footLeft < m_footThreshold && m_footRight < m_footThreshold){
		return GPGESTURE_BASIC;
	}
	
	return GPGESTURE_JUMP;
}

int GPGesutureRecognition::FromDown(){
	if (m_head > m_headThreshold){
		
		return GPGESTURE_BASIC;
	}
	
	return GPGESTURE_DOWN;
}

int GPGesutureRecognition::FromHand(){
	return GPGESTURE_BASIC;
}
