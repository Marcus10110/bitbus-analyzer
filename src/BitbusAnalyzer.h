#ifndef BITBUS_ANALYZER_H
#define BITBUS_ANALYZER_H

#include <Analyzer.h>
#include "BitbusAnalyzerResults.h"
#include "BitbusSimulationDataGenerator.h"

struct BitbusByte
{
	U64 startSample;
	U64 endSample;
	U8 value;
	bool escaped;
};

class BitbusAnalyzerSettings;
class ANALYZER_EXPORT BitbusAnalyzer : public Analyzer2
{
public:
	BitbusAnalyzer();
	virtual ~BitbusAnalyzer();
	virtual void WorkerThread();

	virtual U32 GenerateSimulationData ( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
	virtual U32 GetMinimumSampleRateHz();

	virtual const char* GetAnalyzerName() const;
	virtual bool NeedsRerun();

        virtual void SetupResults();

protected:

	void SetupAnalyzer();

	// Functions to read and process a BITBUS frame
	void ProcessBITBUSFrame();
	BitbusByte ProcessFlags();
	void ProcessAddressField ( BitbusByte byteAfterFlag );
	void ProcessInfoAndFcsField();
	vector<BitbusByte> ReadProcessAndFcsField();
	void InfoAndFcsField ( const vector<BitbusByte> & informationAndFcs );
	void ProcessInformationField ( const vector<BitbusByte> & information );
	void ProcessFcsField ( const vector<BitbusByte> & fcs );
	BitbusByte ReadByte();

	// Bit Sync Transmission functions
	void BitSyncProcessFlags();
	BitState BitSyncReadBit();
	BitbusByte BitSyncReadByte();
	BitbusByte BitSyncProcessFirstByteAfterFlag ( BitbusByte firstAddressByte );
	bool FlagComing();
        bool AbortComing();
	// Byte Async Transmission functions
	BitbusByte ByteAsyncProcessFlags();
	void GenerateFlagsFrames ( vector<BitbusByte> readBytes ) ;
	BitbusByte ByteAsyncReadByte();
	BitbusByte ByteAsyncReadByte_();
        bool IsNRZ();
        bool IsBitSync();
	// Helper functions
	Frame CreateFrame ( U8 mType, U64 mStartingSampleInclusive, U64 mEndingSampleInclusive,
	                    U64 mData1=0, U64 mData2=0, U8 mFlags=0 ) const;
	vector<U8> BitbusBytesToVectorBytes ( const vector<BitbusByte> & asyncBytes ) const;
	U64 VectorToValue ( const vector<U8> & v ) const;

	void AddFrameToResults ( const Frame & frame );
	void CommitFrames();

protected:

	std::auto_ptr< BitbusAnalyzerSettings > mSettings;
	std::auto_ptr< BitbusAnalyzerResults > mResults;
	AnalyzerChannelData* mBitbus;

	U32 mSampleRateHz;
	U64 mSamplesInHalfPeriod;
	U64 mSamplesInAFlag;
	U64 mSamplesInAbort;
	U32 mSamplesIn8Bits;

	vector<U8> mCurrentFrameBytes;

        BitState mPreviousBitState;
        BitState mPreviousLineBitState;

	U32 mConsecutiveOnes;
	bool mReadingFrame;
	bool mAbortFrame;
	bool mFoundEndFlag;

	Frame mEndFlagFrame;
	Frame mAbtFrame;

	vector<Frame> mResultFrames;

	BitbusSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer ( Analyzer* analyzer );

#endif //BITBUS_ANALYZER_H
