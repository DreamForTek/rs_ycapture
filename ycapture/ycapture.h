#pragma once

// ycapture.h : Filter definition

// Filter name
#define NAME_CAPTURESOURCE L"RealSenseDS"

// {5C2CD55C-92AD-4999-8666-912BD3E70001}
DEFINE_GUID(CLSID_CaptureSource, 
	0x5c2cd55c, 0x92ad, 0x4999, 0x86, 0x66, 0x91, 0x2b, 0xd3, 0xe7, 0x0, 0x1);

// Events and memory mapped file name
// Same names must be used in both sender and receiver side.

// �������݂������C�x���g��
#define CS_EVENT_WRITE	L"RS_CaptureSource_Write"
// �f�[�^�ǂݍ��݂������~���[�e�b�N�X��
#define CS_EVENT_READ	L"RS_CaptureSource_Read"
// ���L�f�[�^�t�@�C����(�������}�b�v�h�t�@�C��)
#define CS_SHARED_PATH	L"RS_CaptureSource_Data"
