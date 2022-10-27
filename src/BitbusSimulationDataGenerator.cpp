#include "BitbusSimulationDataGenerator.h"
#include "BitbusAnalyzerSettings.h"
#include <AnalyzerHelpers.h>
#include <algorithm>

BitbusSimulationDataGenerator::BitbusSimulationDataGenerator() :
	mSettings ( 0 ), mSimulationSampleRateHz ( 0 ), mFrameNumber ( 0 ), 
	mWrongFramesSeparation ( 0 ), mAddressByteValue ( 0 ),
	mSamplesInHalfPeriod ( 0 ),
	mSamplesInAFlag ( 0 )

{
}

BitbusSimulationDataGenerator::~BitbusSimulationDataGenerator()
{
}

void BitbusSimulationDataGenerator::Initialize ( U32 simulation_sample_rate, BitbusAnalyzerSettings* settings )
{
	mSimulationSampleRateHz = simulation_sample_rate;
	mSettings = settings;

	mBitbusSimulationData.SetChannel ( mSettings->mInputChannel );
	mBitbusSimulationData.SetSampleRate ( simulation_sample_rate );
	mBitbusSimulationData.SetInitialBitState ( BIT_LOW );

	// Initialize rng seed
	srand ( 5 );

	mSamplesInHalfPeriod = U64 ( simulation_sample_rate / double ( mSettings->mBitRate ) );
	mSamplesInAFlag = mSamplesInHalfPeriod * 7;

	mBitbusSimulationData.Advance ( mSamplesInHalfPeriod * 8 ); // Advance 4 periods
	mFrameNumber = 0;
	mWrongFramesSeparation = ( rand() % 10 ) + 10; // [15..30]

	mAddressByteValue=0;
}

U32 BitbusSimulationDataGenerator::GenerateSimulationData ( U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel )
{
	U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample ( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

	while ( mBitbusSimulationData.GetCurrentSampleNumber() < adjusted_largest_sample_requested )
	{

		// Two consecutive flags
		CreateFlag();
		CreateFlag();

		U32 sizeOfInformation = 4;
		U64 addressBytes = 1;

		vector<U8> address = GenAddressField ( mSettings->mBitbusAddressingMode, addressBytes, mAddressByteValue++ );
		vector<U8> information;

		CreateBITBUSFrame ( address, information );

		// Two consecutive flags
		CreateFlag();
		CreateFlag();

		mFrameNumber++;
	}

	*simulation_channel = &mBitbusSimulationData;
	return 1;
}

void BitbusSimulationDataGenerator::CreateFlag()
{
	if ( mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BIT_SYNC )
	{
		CreateFlagBitSeq();
	}
	else // BITBUS_TRANSMISSION_BYTE_ASYNC
	{
		CreateAsyncByte ( BITBUS_FLAG_VALUE );
	}
}


vector<U8> BitbusSimulationDataGenerator::GenAddressField ( BitbusAddressingMode addressingMode,
        U64 addressBytes,
        U8 value ) const
{
	vector<U8> addrRet;
	if ( addressingMode == BITBUS_ADDRESS_SOF )
	{
		addrRet.push_back ( 0x01 );
	} else {
		addrRet.push_back ( 0x00 );
	}

	addrRet.push_back ( value );

	return addrRet;
}

void BitbusSimulationDataGenerator::CreateBITBUSFrame ( const vector<U8> & address,
        const vector<U8> & information )
{
	vector<U8> allFields;

	allFields.insert ( allFields.end(), address.begin(), address.end() );
	allFields.insert ( allFields.end(), information.begin(), information.end() );

	// Calculate the crc of the address, control and data fields
	vector<U8> fcs = GenFcs ( allFields );
	allFields.insert ( allFields.end(), fcs.begin(), fcs.end() );

	// Transmit the frame in bit-sync or byte-async
	if ( mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BIT_SYNC )
	{
		TransmitBitSync ( allFields );
	}
	else
	{
		TransmitByteAsync ( allFields );
	}
}

vector<U8> BitbusSimulationDataGenerator::GenFcs ( const vector<U8> & stream ) const
{
	vector<U8> crcRet;

	crcRet = Crc16 ( stream );
	
	return crcRet;
}

void BitbusSimulationDataGenerator::TransmitBitSync ( const vector<U8> & stream )
{
	// Opening flag
	CreateFlagBitSeq();

	U8 consecutiveOnes = 0;
	BitState previousBit = BIT_LOW;
	// For each byte of the stream
	U32 index=0;
	for ( U32 s=0; s<stream.size(); ++s )
	{
		// For each bit of the byte stream
		BitExtractor bit_extractor ( stream[ s ], AnalyzerEnums::LsbFirst, 8 );
		for ( U32 i=0; i<8; ++i )
		{
			BitState bit = bit_extractor.GetNextBit();
			CreateSyncBit ( bit );

			if ( bit == BIT_HIGH )
			{
				if ( previousBit == BIT_HIGH )
				{
					consecutiveOnes++;
				}
				else
				{
					consecutiveOnes = 0;
				}
			}
			else // bit low
			{
				consecutiveOnes = 0;
			}

			if ( consecutiveOnes == 4 ) // if five 1s in a row, then insert a 0 and continue
			{
				CreateSyncBit ( BIT_LOW );
				consecutiveOnes = 0;
				previousBit = BIT_LOW;
			}
			else
			{
				previousBit = bit;
			}
			index++;
		}
	}

	// Closing flag
	CreateFlagBitSeq();

}

void BitbusSimulationDataGenerator::CreateFlagBitSeq()
{
	mBitbusSimulationData.Transition();

	mBitbusSimulationData.Advance ( mSamplesInAFlag );
	mBitbusSimulationData.Transition();

	mBitbusSimulationData.Advance ( mSamplesInHalfPeriod );

}

// Maps the bit to the signal using NRZI
void BitbusSimulationDataGenerator::CreateSyncBit ( BitState bitState )
{
	if ( bitState == BIT_LOW ) // BIT_LOW == transition, BIT_HIGH == no transition
	{
		mBitbusSimulationData.Transition();
	}
	mBitbusSimulationData.Advance ( mSamplesInHalfPeriod );
}

void BitbusSimulationDataGenerator::TransmitByteAsync ( const vector<U8> & stream )
{
	// Opening flag
	CreateAsyncByte ( BITBUS_FLAG_VALUE );

	for ( U32 i=0; i < stream.size(); ++i )
	{

		const U8 byte = stream[ i ];
		switch ( byte )
		{
		case BITBUS_FLAG_VALUE: // 0x7E
			CreateAsyncByte ( BITBUS_ESCAPE_SEQ_VALUE );			// 7D escape
			CreateAsyncByte ( BitbusAnalyzerSettings::Bit5Inv ( BITBUS_FLAG_VALUE ) );		// 5E
			break;
		case BITBUS_ESCAPE_SEQ_VALUE: // 0x7D
			CreateAsyncByte ( BITBUS_ESCAPE_SEQ_VALUE );			// 7D escape
			CreateAsyncByte ( BitbusAnalyzerSettings::Bit5Inv ( BITBUS_ESCAPE_SEQ_VALUE ) );	// 5D
			break;
		default:
			CreateAsyncByte ( byte );							// normal byte
		}

		// Fill between bytes (0 to 7 bits of value 1)
		AsyncByteFill ( rand() % 8 );
	}

	// Closing flag
	CreateAsyncByte ( BITBUS_FLAG_VALUE );

}

void BitbusSimulationDataGenerator::AsyncByteFill ( U32 N )
{
	// 0) If the line is not high we must set it high
	if ( mBitbusSimulationData.GetCurrentBitState() == BIT_LOW )
	{
		mBitbusSimulationData.Transition();
	}
	// 1) Fill N high periods
	mBitbusSimulationData.Advance ( mSamplesInHalfPeriod * N );
}

// ISO/IEC 13239:2002(E) page 17
void BitbusSimulationDataGenerator::CreateAsyncByte ( U8 byte )
{

	// 0) If the line is not high we must set it high
	if ( mBitbusSimulationData.GetCurrentBitState() == BIT_LOW )
	{
		mBitbusSimulationData.Transition();
		mBitbusSimulationData.Advance ( mSamplesInHalfPeriod );
	}

	// 1) Start bit (BIT_HIGH -> BIT_LOW)
	mBitbusSimulationData.TransitionIfNeeded ( BIT_LOW );
	mBitbusSimulationData.Advance ( mSamplesInHalfPeriod );

	// 2) Transmit byte
	BitExtractor bit_extractor ( byte, AnalyzerEnums::LsbFirst, 8 );
	for ( U32 i=0; i < 8; ++i )
	{
		BitState bit = bit_extractor.GetNextBit();
		mBitbusSimulationData.TransitionIfNeeded ( bit );
		mBitbusSimulationData.Advance ( mSamplesInHalfPeriod );
	}

	// 3) Stop bit (BIT_LOW -> BIT_HIGH)
	mBitbusSimulationData.TransitionIfNeeded ( BIT_HIGH );
	mBitbusSimulationData.Advance ( mSamplesInHalfPeriod );

}

//
////////////////////// Static functions /////////////////////////////////////////////////////
//

vector<BitState> BitbusSimulationDataGenerator::BytesVectorToBitsVector ( const vector<U8> & v, U32 numberOfBits )
{
	vector<BitState> bitsRet;
	U32 vectorIndex = 0;
	U8 byte;
	bool getByte = true;
	U8 bytePos = 0x80;
	for ( U32 i=0; i < numberOfBits; ++i )
	{
		if ( getByte )
		{
			byte = v.at ( vectorIndex );
			bytePos = 0x80;
			vectorIndex++;
		}

		BitState bit = ( byte & bytePos ) ? BIT_HIGH : BIT_LOW;
		bitsRet.push_back ( bit );

		bytePos >>= 1;

		getByte = ( ( i+1 ) % 8 == 0 );

	}

	return bitsRet;
}

static void CRC16_CCITT__Update(U16 *crc, U8 data)
{
    data ^= (*crc)&0xff;
    data ^= data << 4;
    *crc = ((((U16)data << 8) | (((*crc)>>8)&0xff)) ^ (U16)(data >> 4)
            ^ ((U16)data << 3));
}

vector<U8> BitbusSimulationDataGenerator::Crc16 ( const vector<U8> & stream )
{
	U16 crc = 0xffff;
	vector<U8>::const_iterator i;

	for (i=stream.begin();i!=stream.end();i++) {
		CRC16_CCITT__Update( &crc, *i);
	}
	crc ^= 0xffff;

	vector<U8> crc16Ret;
	crc16Ret.push_back(crc);
	crc16Ret.push_back(crc>>8);

	return crc16Ret;
}
