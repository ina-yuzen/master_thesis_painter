#pragma once

typedef enum _GPGestueElements{
	GPGESTURE_BASIC =0x0000,
	GPGESTURE_WALK = 0x0001,
	GPGESTURE_JUMP = 0x0002,
	GPGESTURE_DOWN = 0x0004,
	GPGESTURE_HAND = 0x0008,
	GPGESTURE_FAULT =0x1000,
}GPGestureElements;

class GPGestureState{
	int type;
	bool chenge;
public:
	void Set(int t, bool b){ type = t, chenge = b; };
};

class GPGesutureRecognition
{
public:
	bool m_bBasicPose;
	int m_currentPose;
	int m_previosPose;
	int m_nWalkCount;
	int m_nJumpCount;
	int m_nDownCount;
	int m_nHandCount;
	float m_head, m_footLeft, m_footRight;

	float m_footThreshold;
	float m_headThreshold;

	int m_handState;

public:
	GPGestureState RecognizeGesture(float head, float footl, float footr, float handR, float handL);

	int GetWalkCount() { return m_nWalkCount; };
	int GetJumpCount() { return m_nJumpCount; };
	int GetDownCount() { return m_nDownCount; };
	int GetHandCount() { return m_nHandCount; };

	int GetHandState() { return m_handState; };

protected:
	int FromBasic();
	int FromWalk();
	int FromJump();
	int FromDown();
	int FromHand();

public:
	GPGesutureRecognition ();
	virtual ~GPGesutureRecognition();
};

