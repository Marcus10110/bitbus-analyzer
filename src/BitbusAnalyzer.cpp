#include "BitbusAnalyzer.h"
#include "BitbusAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>
#include <stdio.h>

using namespace std;

#undef DEBUGENABLED

#ifdef DEBUGENABLED
#include <stdarg.h>

#define DBG(x,...)

static FILE *debugfile = NULL;
static const char debugfilename[] = "C:\\BITBUS\\BITBUS.txt";

void do_debug(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (debugfile == NULL) {
		debugfile = fopen(debugfilename,"w");
	}
	if (debugfile) {
		vfprintf(debugfile, fmt, ap);
		fputs("\r\n", debugfile);
		fflush(debugfile);
#ifndef __linux__
		_flushall();
#endif

	}
	va_end(ap);
}

#else
#define DBG(x,...) /* */
#endif



BitbusAnalyzer::BitbusAnalyzer()
    :	Analyzer2(),
        mSettings ( new BitbusAnalyzerSettings() ),
        mSimulationInitilized ( false ),
        mResults ( 0 ), mBitbus ( 0 ),
        mSampleRateHz ( 0 ), mSamplesInHalfPeriod ( 0 ), mSamplesInAFlag ( 0 ), mSamplesInAbort ( 0 ), mSamplesIn8Bits ( 0 ),
        mPreviousBitState ( BIT_LOW ), mConsecutiveOnes ( 0 ), mReadingFrame ( false ),mAbortFrame ( false ),
        mFoundEndFlag ( false ),
        mEndFlagFrame(), mAbtFrame()
{
	DBG("Instantiating new BITBUS analyzer");
	SetAnalyzerSettings ( mSettings.get() );
}

bool BitbusAnalyzer::IsNRZ()
{
        return mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BIT_SYNC_NRZ;
}

bool BitbusAnalyzer::IsBitSync()
{
        return mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BIT_SYNC_NRZ ||
                mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BIT_SYNC;
}

BitbusAnalyzer::~BitbusAnalyzer()
{
	KillThread();
}

void BitbusAnalyzer::SetupAnalyzer()
{
	DBG("Setting up analyzer");
        mBitbus = GetAnalyzerChannelData ( mSettings->mInputChannel );

        double halfPeriod = ( 1.0 / double ( mSettings->mBitRate ) ) * 1000000.0;
        mSampleRateHz = GetSampleRate();
        mSamplesInHalfPeriod = U64 ( ( mSampleRateHz * halfPeriod ) / 1000000.0 );

        if (IsNRZ()) {
                mSamplesInAFlag = mSamplesInHalfPeriod * 6;
        } else {
                mSamplesInAFlag = mSamplesInHalfPeriod * 7;
        }

        mSamplesInAbort = mSamplesInHalfPeriod * 7;

        mSamplesIn8Bits = mSamplesInHalfPeriod * 8;

        mPreviousBitState = mBitbus->GetBitState();
        mConsecutiveOnes = 0;
        mReadingFrame = false;
	mAbortFrame = false;

        mFoundEndFlag = false;

        mResultFrames.clear();
        mCurrentFrameBytes.clear();
        DBG("Analyzer setup finished");
}

void BitbusAnalyzer::CommitFrames()
{
	DBG("Committing Frames");
	for ( U32 i=0; i < mResultFrames.size(); ++i )
	{
		Frame frame = mResultFrames.at ( i );
		mResults->AddFrame ( frame );
	}
	DBG("Frames commited.");
	
}

void BitbusAnalyzer::WorkerThread()
{
	SetupAnalyzer();

	DBG("IsBitSync()");
	if ( IsBitSync() )
	{
		// Synchronize
		mBitbus->AdvanceToNextEdge();
	}
	DBG("Enter main loop");
	// Main loop
	for ( ; ; )
	{
		ProcessBITBUSFrame();
		DBG("Committing frames");
		CommitFrames();
		DBG("Clearing result frame");
		mResultFrames.clear();
		DBG("Commiting result");
		mResults->CommitResults();
		DBG("Reporting progress");
		ReportProgress ( mBitbus->GetSampleNumber() );
		DBG("Check for exit");
		CheckIfThreadShouldExit();
	}

}

//
/////////////// SYNC BIT TRAMISSION ///////////////////////////////////////////////
//

void BitbusAnalyzer::ProcessBITBUSFrame()
{
	bool earlyAbort;
	mCurrentFrameBytes.clear();
	DBG("ProcessFlags");
	BitbusByte addressByte = ProcessFlags();
	earlyAbort = mAbortFrame;
	DBG("ProcessAddres");
	ProcessAddressField ( addressByte );
	DBG("ProcessInfo");
	ProcessInfoAndFcsField();

	if ( mAbortFrame && (!earlyAbort) ) // The frame has been aborted at some point
	{
		AddFrameToResults ( mAbtFrame );
	}
	else
	{
		AddFrameToResults ( mEndFlagFrame );
	}
	if ( mAbortFrame ) {
		if ( IsBitSync() )
		{
			// After abortion, synchronize again
			DBG("Resync after abort");
			mBitbus->AdvanceToNextEdge();
			DBG("Resynced");
		}
	}
	// Reset state bool variables
	mReadingFrame = false;
	mAbortFrame = false;
	//}
	DBG("Leaving processBITBUSFrame");
}

BitbusByte BitbusAnalyzer::ProcessFlags()
{
        BitbusByte addressByte;
	if ( IsBitSync() )
	{
		BitSyncProcessFlags();
		mReadingFrame = true;
		addressByte = ReadByte();
	}
	else
	{
		mReadingFrame = true;
		addressByte = ByteAsyncProcessFlags();
	}
	return addressByte;
}

bool BitbusAnalyzer::AbortComing()
{
        if (IsNRZ()) {
                if (mBitbus->GetBitState()==BIT_LOW)
                        return false;
	}
	DBG("Check trans");

	bool transition = mBitbus->WouldAdvancingCauseTransition ( mSamplesInAbort + mSamplesInHalfPeriod * 0.5 );
	DBG("Trans %d\n", transition);
	return !transition;
}

// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
void BitbusAnalyzer::BitSyncProcessFlags()
{
	bool flagEncountered = false;
        vector<BitbusByte> flags;
        mAbortFrame = false;

	for ( ; ; )
	{
		DBG("Check for abort");
		if ( AbortComing() )
		{
			DBG("Abort seen");
			// Show fill flags
			for ( U32 i=0; i < flags.size(); ++i )
			{
				Frame frame = CreateFrame ( BITBUS_FIELD_FLAG, flags.at ( i ).startSample,
				                            flags.at ( i ).endSample, BITBUS_FLAG_FILL );
				AddFrameToResults ( frame );
			}
                        flags.clear();
#if 0
			mAbtFrame = CreateFrame ( BITBUS_ABORT_SEQ, mBitbus->GetSampleNumber(), mBitbus->GetSampleNumber() + mSamplesIn8Bits );
#endif
                        //mBitbus->AdvanceToNextEdge();
                        flagEncountered = false;
			mAbortFrame = true;

                        break;
                }

		DBG("Check for flag");
		if ( FlagComing() )
		{
			DBG("Flag coming");
			BitbusByte bs;
			bs.value = 0;

                        bs.startSample = mBitbus->GetSampleNumber();
                        // Move back to start of bit sequence
                        bs.startSample -= mSamplesInHalfPeriod;
			DBG("Advance");
			mBitbus->AdvanceToNextEdge();
			bs.endSample = mBitbus->GetSampleNumber();
                        bs.endSample += mSamplesInHalfPeriod;

			flags.push_back ( bs );

			if ( mBitbus->WouldAdvancingCauseTransition ( mSamplesInHalfPeriod * 1.5 ) )
			{
				mBitbus->Advance ( mSamplesInHalfPeriod * 0.5 );
				mPreviousBitState = mBitbus->GetBitState();
				mBitbus->AdvanceToNextEdge();
			}
			else
			{
				mBitbus->Advance ( mSamplesInHalfPeriod * 0.5 );
				mPreviousBitState = mBitbus->GetBitState();
				mBitbus->Advance ( mSamplesInHalfPeriod * 0.5 );
			}
			DBG("Mark flag");
			flagEncountered = true;
		}
		else // non-flag
		{
			DBG("No flag ");
			if ( flagEncountered )
			{
				DBG("But flag Encountered");
				break;
			}
			else // non-flag byte before a byte-flag is ignored
			{
				DBG("Non-flag, advance");
				mBitbus->AdvanceToNextEdge();
			}
		}
	}

	if ( !mAbortFrame )
	{
		DBG("Good frame");
                for ( U32 i=0; i < flags.size(); ++i )
                {
                        Frame frame = CreateFrame ( BITBUS_FIELD_FLAG, flags.at ( i ).startSample,
                                                   flags.at ( i ).endSample, BITBUS_FLAG_FILL );
                        if ( i == flags.size() - 1 )
                        {
                                frame.mData1 = BITBUS_FLAG_START;
                        }
                        AddFrameToResults ( frame );
                }
        }
        mConsecutiveOnes=0;
}

// Read bit with bit-stuffing
BitState BitbusAnalyzer::BitSyncReadBit()
{
	DBG("BitSyncReadBit");
	// Re-sync
	if ( mBitbus->GetSampleOfNextEdge() < mBitbus->GetSampleNumber() + mSamplesInHalfPeriod * 0.20 )
	{
		mBitbus->AdvanceToNextEdge();
	}

	BitState ret;

	mBitbus->Advance ( mSamplesInHalfPeriod * 0.5 );
	BitState lineBit = mBitbus->GetBitState(); // sample the bit
        BitState bit;

        // For NRZI we need to change bit state.
        mPreviousLineBitState = lineBit;

        bit = lineBit;

        if (!IsNRZ()) {
                if (bit==mPreviousBitState)
                        bit=BIT_HIGH;
                else
                        bit=BIT_LOW;
        }

	if ( bit == BIT_HIGH )
	{
		mConsecutiveOnes++;
		DBG("OneBit");
		if ( mReadingFrame && mConsecutiveOnes == 5 )
		{
			DBG("Need de-stuffing");
			U64 currentPos = mBitbus->GetSampleNumber();
			
			// Check for 0-bit insertion (i.e. line toggle)
			if ( mBitbus->GetSampleOfNextEdge() < currentPos + mSamplesInHalfPeriod )
			{
				// Advance to the next edge to re-synchronize the analyzer
				mBitbus->AdvanceToNextEdge();
				// Mark the bit-stuffing
				mResults->AddMarker ( mBitbus->GetSampleNumber() , AnalyzerResults::Dot, mSettings->mInputChannel );
				mBitbus->Advance ( mSamplesInHalfPeriod * 0.5 );

				mPreviousBitState = mBitbus->GetBitState();
				mConsecutiveOnes = 0;
			}
			else // Invalid frame...
			{
                                mConsecutiveOnes = 0;
                                mAbtFrame = CreateFrame ( BITBUS_ABORT_SEQ, mBitbus->GetSampleNumber(), mBitbus->GetSampleNumber() + mSamplesIn8Bits );
				mAbortFrame = true;
			}

		}
		else
		{
			mPreviousBitState = lineBit;
		}
	}
	else // bit changed so it's a 0
	{
		DBG("ZeroBit");
		mConsecutiveOnes = 0;
		mPreviousBitState = lineBit;
        }

        ret = bit;
	
	mBitbus->Advance ( mSamplesInHalfPeriod * 0.5 );
	DBG("Resync");
	// Re-sync
	if ( mBitbus->GetSampleOfNextEdge() < mBitbus->GetSampleNumber() + mSamplesInHalfPeriod * 0.20 )
	{
		mBitbus->AdvanceToNextEdge();
	}

	return ret;
}

bool BitbusAnalyzer::FlagComing()
{
        // NRZ first
        // 01111110
        // We are at 0->1 transition. If we find the edge at 1->0 transition we
        // have a flag.
	DBG("Flag coming check");
        bool validEdge = (!mBitbus->WouldAdvancingCauseTransition ( mSamplesInAFlag - mSamplesInHalfPeriod  * 0.5 )) &&
                mBitbus->WouldAdvancingCauseTransition ( mSamplesInAFlag + mSamplesInHalfPeriod * 0.5 );
		DBG("Valid edge is %d", validEdge);
        if (validEdge && IsNRZ()) {
                if (mBitbus->GetBitState()==BIT_LOW) {
						DBG("No flag coming");
                        return false;
                }
        }
	DBG("Flag coming check is %d", (int)validEdge);
        return validEdge;
}

BitbusByte BitbusAnalyzer::BitSyncReadByte()
{
	DBG("BitSyncReadByte");
	if ( mReadingFrame && AbortComing() )
	{
		// Create "Abort Frame" frame
		U64 startSample = mBitbus->GetSampleNumber();
		mBitbus->Advance ( mSamplesIn8Bits );
		U64 endSample = mBitbus->GetSampleNumber();

		mAbtFrame = CreateFrame ( BITBUS_ABORT_SEQ, startSample + mSamplesInHalfPeriod, endSample );
		mAbortFrame = true;
		BitbusByte b;
		b.startSample = 0;
		b.endSample = 0;
		b.value = 0;
		return b;
	}
	DBG("CheckForFlag");
	if ( mReadingFrame && FlagComing() && !IsNRZ())
	{
		U64 startSample = mBitbus->GetSampleNumber() - mSamplesInHalfPeriod;
		mBitbus->AdvanceToNextEdge();
		U64 endSample = mBitbus->GetSampleNumber() + mSamplesInHalfPeriod;
		mFoundEndFlag = true;
		BitbusByte bs = { startSample, endSample, BITBUS_FLAG_VALUE };
		return bs;
	}

	U64 byteValue= 0;
	DataBuilder dbyte;
	dbyte.Reset ( &byteValue, AnalyzerEnums::LsbFirst, 8 );
	U64 startSample = mBitbus->GetSampleNumber();
	for ( U32 i=0; i < 8 ; ++i )
	{
		DBG("Read bit %d", (int)i);
                BitState bit = BitSyncReadBit();
                if (i==0 && IsNRZ()) {
                        // we may be reading a frame now.
                        if (FlagComing()) {
                                U64 startSample = mBitbus->GetSampleNumber() - mSamplesInHalfPeriod;;
                                mBitbus->AdvanceToNextEdge();
                                U64 endSample = mBitbus->GetSampleNumber() + mSamplesInHalfPeriod;;
                                mFoundEndFlag = true;
                                BitbusByte bs = { startSample, endSample, BITBUS_FLAG_VALUE };
                                return bs;
                        }
                }
		if ( mAbortFrame )
		{
			DBG("Is abort, leave");
			BitbusByte b;
			b.startSample = 0;
			b.endSample = 0;
			b.value = 0;
			return b;
		}
		dbyte.AddBit ( bit );
	}
	U64 endSample = mBitbus->GetSampleNumber();
	BitbusByte bs = { startSample, endSample, U8 ( byteValue ), false };
	DBG("Read byte 0x%02x", (unsigned)bs.value);
	mCurrentFrameBytes.push_back ( bs.value );
	return bs;
}

//
/////////////// ASYNC BYTE TRAMISSION ///////////////////////////////////////////////
//

// Interframe time fill: ISO/IEC 13239:2002(E) pag. 21
BitbusByte BitbusAnalyzer::ByteAsyncProcessFlags()
{
	bool flagEncountered = false;
	// Read bytes until non-flag byte
	vector<BitbusByte> readBytes;

	for ( ; ; )
	{
		BitbusByte asyncByte = ReadByte();
		if ( ( asyncByte.value != BITBUS_FLAG_VALUE ) && flagEncountered )
		{
			readBytes.push_back ( asyncByte );
			break;
		}
		else if ( asyncByte.value == BITBUS_FLAG_VALUE )
		{
			readBytes.push_back ( asyncByte );
			flagEncountered = true;
		}
		if ( mAbortFrame )
		{
			GenerateFlagsFrames ( readBytes );
			BitbusByte b;
			b.startSample = 0;
			b.endSample = 0;
			b.value = 0;
			return b;
		}

	}

	GenerateFlagsFrames ( readBytes );

	BitbusByte nonFlagByte = readBytes.back();
	return nonFlagByte;

}

void BitbusAnalyzer::GenerateFlagsFrames ( vector<BitbusByte> readBytes )
{
	// 2) Generate the flag frames and return non-flag byte after the flags
	for ( U32 i=0; i<readBytes.size()-1; ++i )
	{
		BitbusByte asyncByte = readBytes[ i ];

		Frame frame = CreateFrame ( BITBUS_FIELD_FLAG, asyncByte.startSample, asyncByte.endSample );

		if ( i == readBytes.size() - 2 ) // start flag
		{
			frame.mData1 = BITBUS_FLAG_START;
		}
		else // fill flag
		{
			frame.mData1 = BITBUS_FLAG_FILL;
		}

		AddFrameToResults ( frame );
	}
}

void BitbusAnalyzer::ProcessAddressField ( BitbusByte byteAfterFlag )
{
        U8 addressSize = 2;
        BitbusByte addressByte = byteAfterFlag;
        BitbusByte startByte = byteAfterFlag;
        U64 addressValue = 0;

	if ( mAbortFrame )
	{
		return;
	}

        addressByte = ReadByte();
        switch (mSettings->mBitbusAddressingMode) {
        case BITBUS_ADDRESS_SOF:
            {
                addressSize--;
                U8 flag = ( byteAfterFlag.escaped ) ? BITBUS_ESCAPED_BYTE : 0;

		Frame frame = CreateFrame ( BITBUS_FIELD_SOH, byteAfterFlag.startSample,
		                            byteAfterFlag.endSample, byteAfterFlag.value, 0, flag );
		AddFrameToResults ( frame );
		// Put a marker in the beggining of the BITBUS frame
                //mResults->AddMarker ( frame.mStartingSampleInclusive, AnalyzerResults::Start, mSettings->mInputChannel );
                startByte = addressByte;
            }
        case BITBUS_ADDRESS_EXTENDED:
            {
                addressValue = (byteAfterFlag.value<<8);
                addressValue += addressByte.value;
                Frame frame = CreateFrame ( BITBUS_FIELD_ADDRESS, startByte.startSample,
                                           addressByte.endSample, addressValue, 0 );

                AddFrameToResults ( frame );
            }
            break;
        case BITBUS_ADDRESS_ADDR_RESERVED:
            {
                U8 flag = ( byteAfterFlag.escaped ) ? BITBUS_ESCAPED_BYTE : 0;

                Frame frame = CreateFrame ( BITBUS_FIELD_ADDRESS, byteAfterFlag.startSample,
                                     byteAfterFlag.endSample, byteAfterFlag.value, 0, flag );
                AddFrameToResults ( frame );


                frame = CreateFrame ( BITBUS_FIELD_RESERVED, addressByte.startSample,
		                            addressByte.endSample, addressByte.value, 0, 0 );
		AddFrameToResults ( frame );

            }
            break;
        default:
            break;
        }
}

vector<BitbusByte> BitbusAnalyzer::ReadProcessAndFcsField()
{
	vector<BitbusByte> infoAndFcs;
        if ( mAbortFrame )
        {
                return infoAndFcs;
        }

	for ( ; ; )
	{
		BitbusByte asyncByte = ReadByte();
                if ( mAbortFrame )
		{
			return infoAndFcs;
		}
		if ( ( asyncByte.value == BITBUS_FLAG_VALUE ) && mFoundEndFlag ) // End of frame found
		{
			mEndFlagFrame = CreateFrame ( BITBUS_FIELD_FLAG, asyncByte.startSample, asyncByte.endSample, BITBUS_FLAG_END );
			mFoundEndFlag = false;
			break;
		}
		else  // information or fcs byte
		{
			infoAndFcs.push_back ( asyncByte );
                }
        }

        return infoAndFcs;

}

void BitbusAnalyzer::ProcessInfoAndFcsField()
{
	vector<BitbusByte> informationAndFcs = ReadProcessAndFcsField();

	InfoAndFcsField ( informationAndFcs );
}

void BitbusAnalyzer::InfoAndFcsField ( const vector<BitbusByte> & informationAndFcs )
{
        vector<BitbusByte> information = informationAndFcs;
        vector<BitbusByte> fcs;
        if (!mAbortFrame ) {
                if ( information.size() >= 2 )
                {
                        fcs.insert ( fcs.end(), information.end()-2, information.end() );
                        information.erase ( information.end()-2, information.end() );
                }

        }
        ProcessInformationField ( information );

        if ( !mAbortFrame )
        {
                if ( !fcs.empty() )
                {
                        ProcessFcsField ( fcs );
                }
        }

}

void BitbusAnalyzer::ProcessInformationField ( const vector<BitbusByte> & information )
{
	for ( U32 i=0; i<information.size(); ++i )
	{
		BitbusByte byte = information.at ( i );
		U8 flag = ( byte.escaped ) ? BITBUS_ESCAPED_BYTE : 0;
		Frame frame = CreateFrame ( BITBUS_FIELD_INFORMATION, byte.startSample,
		                            byte.endSample, byte.value, i, flag );
		AddFrameToResults ( frame );
	}
}

void BitbusAnalyzer::AddFrameToResults ( const Frame & frame )
{
	mResultFrames.push_back ( frame );
}

void BitbusAnalyzer::ProcessFcsField ( const vector<BitbusByte> & fcs )
{
	vector<U8> calculatedFcs;
	vector<U8> readFcs = BitbusBytesToVectorBytes ( fcs );

        if ( mCurrentFrameBytes.size() >= 2 )
        {
                mCurrentFrameBytes.erase ( mCurrentFrameBytes.end()-2, mCurrentFrameBytes.end() );
        }
        calculatedFcs = BitbusSimulationDataGenerator::Crc16 ( mCurrentFrameBytes );

	Frame frame = CreateFrame ( BITBUS_FIELD_FCS, fcs.front().startSample, fcs.back().endSample,
	                            VectorToValue ( readFcs ), VectorToValue ( calculatedFcs ) );

	if ( calculatedFcs != readFcs )
	{
		frame.mFlags = DISPLAY_AS_ERROR_FLAG;
	}

	AddFrameToResults ( frame );

        if ( calculatedFcs != readFcs ) {
                mResults->AddMarker ( frame.mEndingSampleInclusive, AnalyzerResults::ErrorX, mSettings->mInputChannel );
        }
}

BitbusByte BitbusAnalyzer::ReadByte()
{
	return ( mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BYTE_ASYNC )
	       ? ByteAsyncReadByte() : BitSyncReadByte();
}

BitbusByte BitbusAnalyzer::ByteAsyncReadByte()
{
	BitbusByte ret = ByteAsyncReadByte_();

	if ( mReadingFrame && ( ret.value == BITBUS_FLAG_VALUE ) )
	{
		mFoundEndFlag = true;
	}

	// Check for escape character
	if ( mReadingFrame && ( ret.value == BITBUS_ESCAPE_SEQ_VALUE ) ) // escape byte read
	{
		U64 startSampleEsc = ret.startSample;
		ret = ByteAsyncReadByte_();

		if ( ret.value == BITBUS_FLAG_VALUE ) // abort sequence = ESCAPE_BYTE + FLAG_BYTE (0x7D-0x7E)
		{
			// Create "Abort Frame" frame
			mAbtFrame = CreateFrame ( BITBUS_ABORT_SEQ, startSampleEsc, ret.endSample );
			mAbortFrame = true;
			return ret;
		}
		else
		{
			// Real data: with the bit-5 inverted (that's what we use for the crc)
			mCurrentFrameBytes.push_back ( BitbusAnalyzerSettings::Bit5Inv ( ret.value ) );
			ret.startSample = startSampleEsc;
			ret.escaped = true;
			return ret;
		}
	}

	if ( mReadingFrame && ( ret.value != BITBUS_FLAG_VALUE ) )
	{
		mCurrentFrameBytes.push_back ( ret.value );
	}

	return ret;
}

BitbusByte BitbusAnalyzer::ByteAsyncReadByte_()
{
	// Line must be HIGH here
	if ( mBitbus->GetBitState() == BIT_LOW )
	{
		mBitbus->AdvanceToNextEdge();
	}

	mBitbus->AdvanceToNextEdge(); // high->low transition (start bit)

	mBitbus->Advance ( mSamplesInHalfPeriod * 0.5 );

	U64 byteStartSample = mBitbus->GetSampleNumber() + mSamplesInHalfPeriod * 0.5;

	U64 byteValue2= 0;
	DataBuilder dbyte;
	dbyte.Reset ( &byteValue2, AnalyzerEnums::LsbFirst, 8 );

	for ( U32 i=0; i<8 ; ++i )
	{
		mBitbus->Advance ( mSamplesInHalfPeriod );
		dbyte.AddBit ( mBitbus->GetBitState() );
	}

	U8 byteValue = U8 ( byteValue2 );

	U64 byteEndSample = mBitbus->GetSampleNumber() + mSamplesInHalfPeriod * 0.5;

	mBitbus->Advance ( mSamplesInHalfPeriod );

	BitbusByte asyncByte = { byteStartSample, byteEndSample, byteValue, false };

	return asyncByte;
}


//
///////////////////////////// Helper functions ///////////////////////////////////////////
//

// "Ctor" for the Frame class
Frame BitbusAnalyzer::CreateFrame ( U8 mType, U64 mStartingSampleInclusive, U64 mEndingSampleInclusive,
                                  U64 mData1, U64 mData2, U8 mFlags ) const
{
	Frame frame;
	frame.mStartingSampleInclusive = mStartingSampleInclusive;
	frame.mEndingSampleInclusive = mEndingSampleInclusive;
	frame.mType = mType;
	frame.mData1 = mData1;
	frame.mData2 = mData2;
	frame.mFlags = mFlags;
	return frame;
}

vector<U8> BitbusAnalyzer::BitbusBytesToVectorBytes ( const vector<BitbusByte> & asyncBytes ) const
{
	vector<U8> ret;
	for ( U32 i=0; i < asyncBytes.size(); ++i )
	{
		ret.push_back ( asyncBytes[ i ].value );
	}
	return ret;
}

U64 BitbusAnalyzer::VectorToValue ( const vector<U8> & v ) const
{
	U64 value=0;
	U32 j= 8 * ( v.size() - 1 );
	for ( U32 i=0; i < v.size(); ++i )
	{
		value |= ( v.at ( i ) << j );
		j-=8;
	}
	return value;
}

bool BitbusAnalyzer::NeedsRerun()
{
    return false;
}

void BitbusAnalyzer::SetupResults()
{
    mResults.reset ( new BitbusAnalyzerResults ( this, mSettings.get() ) );
    SetAnalyzerResults ( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn ( mSettings->mInputChannel );
}

U32 BitbusAnalyzer::GenerateSimulationData ( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	if ( mSimulationInitilized == false )
	{
		mSimulationDataGenerator.Initialize ( GetSimulationSampleRate(), mSettings.get() );
		mSimulationInitilized = true;
	}

	return mSimulationDataGenerator.GenerateSimulationData ( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 BitbusAnalyzer::GetMinimumSampleRateHz()
{
	return mSettings->mBitRate * 4;
}

const char* BitbusAnalyzer::GetAnalyzerName() const
{
	return "BITBUS";
}

const char* GetAnalyzerName()
{
	return "BITBUS";
}

Analyzer* CreateAnalyzer()
{
	return new BitbusAnalyzer();
}

void DestroyAnalyzer ( Analyzer* analyzer )
{
	delete analyzer;
}
