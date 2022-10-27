#ifndef BITBUS_SIMULATION_DATA_GENERATOR
#define BITBUS_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>
#include "BitbusAnalyzerSettings.h"
#include <string>
#include <vector>

using namespace std;

class BitbusSimulationDataGenerator
{
public:
	BitbusSimulationDataGenerator();
	~BitbusSimulationDataGenerator();

	void Initialize ( U32 simulation_sample_rate, BitbusAnalyzerSettings* settings );
	U32 GenerateSimulationData ( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel );

	static vector<U8> Crc16 ( const vector<U8> & stream );
	static vector<BitState> BytesVectorToBitsVector ( const vector<U8> & v, U32 numberOfBits );

protected:

	void CreateBITBUSFrame ( const vector<U8> & address, const vector<U8> & information );
	void CreateFlag();

	// Sync Transmission
	void CreateFlagBitSeq();
	void CreateSyncBit ( BitState bitState );
	void TransmitBitSync ( const vector<U8> & stream );

	// Async transmission
	void TransmitByteAsync ( const vector<U8> & stream );
	void CreateAsyncByte ( U8 byte );
	void AsyncByteFill ( U32 N );

	// Helper functions
	vector<U8> GenFcs ( const vector<U8> & stream ) const;

	bool ContainsElement ( U32 index ) const;

	vector<U8> GenAddressField ( BitbusAddressingMode addressingMode, U64 addressBytes, U8 value ) const;

	BitbusAnalyzerSettings* mSettings;
	U32 mSimulationSampleRateHz;

	vector<U32> mAbortFramesIndexes;
	U32 mFrameNumber;
	U32 mWrongFramesSeparation;

	U8 mAddressByteValue;

	SimulationChannelDescriptor mBitbusSimulationData;

	U64 mSamplesInHalfPeriod;
	U64 mSamplesInAFlag;

};
#endif //BITBUS_SIMULATION_DATA_GENERATOR
