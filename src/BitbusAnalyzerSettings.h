#ifndef BITBUS_ANALYZER_SETTINGS
#define BITBUS_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

/////////////////////////////////////

// NOTE: terminology:
//    * BITBUS Frame == Saleae Logic Packet
//    * BITBUS Field == Saleae Logic Frame
//    * BITBUS transactions not supported

// Inner frames types of BITBUS frame (address, control, data, fcs, etc)
enum BitbusFieldType {
    BITBUS_FIELD_FLAG,
    BITBUS_FIELD_SOH,
    BITBUS_FIELD_ADDRESS,
    BITBUS_FIELD_PACKET_TYPE,
    BITBUS_FIELD_INFORMATION,
    BITBUS_FIELD_FCS,
    BITBUS_ABORT_SEQ,
    BITBUS_FIELD_RESERVED,
};

//
enum BitbusAddressingMode {
    BITBUS_ADDRESS_SOF,
    BITBUS_ADDRESS_EXTENDED,
    BITBUS_ADDRESS_ADDR_RESERVED,
};

// Transmission mode (bit stuffing or byte stuffing)
enum BitbusTransmissionModeType {
        BITBUS_TRANSMISSION_BIT_SYNC = 0,
        BITBUS_TRANSMISSION_BIT_SYNC_NRZ,
        BITBUS_TRANSMISSION_BYTE_ASYNC
};

// Flag Field Type (Start, End or Fill)
enum BitbusFlagType { BITBUS_FLAG_START = 0, BITBUS_FLAG_END = 1, BITBUS_FLAG_FILL = 2 };

// Special values for Byte Asynchronous Transmission
#define BITBUS_FLAG_VALUE 0x7E
#define BITBUS_FILL_VALUE 0xFF
#define BITBUS_ESCAPE_SEQ_VALUE 0x7F

// For Frame::mFlag
#define BITBUS_ESCAPED_BYTE ( 1 << 0 )

/////////////////////////////////////

class BitbusAnalyzerSettings : public AnalyzerSettings
{
public:
	BitbusAnalyzerSettings();
	virtual ~BitbusAnalyzerSettings();

	virtual bool SetSettingsFromInterfaces();
	void UpdateInterfacesFromSettings();
	virtual void LoadSettings ( const char* settings );
	virtual const char* SaveSettings();

	static U8 Bit5Inv ( U8 value );

	Channel mInputChannel;
	U32 mBitRate;

	BitbusTransmissionModeType mTransmissionMode;
        BitbusAddressingMode mBitbusAddressingMode;

protected:
	std::auto_ptr< AnalyzerSettingInterfaceChannel >	mInputChannelInterface;
	std::auto_ptr< AnalyzerSettingInterfaceInteger >	mBitRateInterface;
	std::auto_ptr< AnalyzerSettingInterfaceNumberList >	mBitbusAddressingModeInterface;
	std::auto_ptr< AnalyzerSettingInterfaceNumberList >	mBitbusTransmissionInterface;
};

#endif //BITBUS_ANALYZER_SETTINGS
