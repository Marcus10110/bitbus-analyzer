#include "BitbusAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

BitbusAnalyzerSettings::BitbusAnalyzerSettings():
	mInputChannel ( UNDEFINED_CHANNEL ),
	mBitRate ( 62500 ),
	mTransmissionMode ( BITBUS_TRANSMISSION_BIT_SYNC ),
	mBitbusAddressingMode ( BITBUS_ADDRESS_SOF )
{
	mInputChannelInterface.reset ( new AnalyzerSettingInterfaceChannel() );
	mInputChannelInterface->SetTitleAndTooltip ( "BITBUS", "Pioneer BitBus" );
	mInputChannelInterface->SetChannel ( mInputChannel );

	mBitRateInterface.reset ( new AnalyzerSettingInterfaceInteger() );
	mBitRateInterface->SetTitleAndTooltip ( "Bit Rate (Bits/S)",  "Specify the bit rate in bits per second." );
	mBitRateInterface->SetMax ( 50000000 );
	mBitRateInterface->SetMin ( 1 );
	mBitRateInterface->SetInteger ( mBitRate );

	mBitbusTransmissionInterface.reset ( new AnalyzerSettingInterfaceNumberList() );
	mBitbusTransmissionInterface->SetTitleAndTooltip ( "Transmission Mode", "Specify the transmission mode of the BITBUS frames" );
	mBitbusTransmissionInterface->AddNumber ( BITBUS_TRANSMISSION_BIT_SYNC, "NRZI Bit Synchronous", "Bit-oriented transmission using bit stuffing and NRZI line encoding" );
	mBitbusTransmissionInterface->AddNumber ( BITBUS_TRANSMISSION_BIT_SYNC_NRZ, "NRZ Bit Synchronous", "Bit-oriented transmission using bit stuffing and NRZ line encoding" );
	mBitbusTransmissionInterface->AddNumber ( BITBUS_TRANSMISSION_BYTE_ASYNC, "Byte Asynchronous", "Byte asynchronous transmission using byte stuffing (Also known as start/stop mode)" );
	mBitbusTransmissionInterface->SetNumber ( mTransmissionMode );

	mBitbusAddressingModeInterface.reset ( new AnalyzerSettingInterfaceNumberList() );
	mBitbusAddressingModeInterface->SetTitleAndTooltip ( "Address Field Type", "Specify the address field type of an BITBUS frame." );
	mBitbusAddressingModeInterface->AddNumber ( BITBUS_ADDRESS_SOF, "SOF", "First octet is SOF, Address is 8-bit" );
	mBitbusAddressingModeInterface->AddNumber ( BITBUS_ADDRESS_ADDR_RESERVED, "Normal", "First octet Address (8 bits), 8-bit reserved field" );
	mBitbusAddressingModeInterface->AddNumber ( BITBUS_ADDRESS_EXTENDED, "Extended", "Extended Address Field (16 bits)" );
	mBitbusAddressingModeInterface->SetNumber ( mBitbusAddressingMode );

	AddInterface ( mInputChannelInterface.get() );
	AddInterface ( mBitRateInterface.get() );
	AddInterface ( mBitbusTransmissionInterface.get() );
	AddInterface ( mBitbusAddressingModeInterface.get() );

	AddExportOption ( 0, "Export as text/csv file" );
	AddExportExtension ( 0, "text", "txt" );
	AddExportExtension ( 0, "csv", "csv" );

	ClearChannels();
	AddChannel ( mInputChannel, "BITBUS", false );
}

BitbusAnalyzerSettings::~BitbusAnalyzerSettings()
{
}

U8 BitbusAnalyzerSettings::Bit5Inv ( U8 value )
{
	return value ^ 0x20;
}

bool BitbusAnalyzerSettings::SetSettingsFromInterfaces()
{
	mInputChannel = mInputChannelInterface->GetChannel();
	mBitRate = mBitRateInterface->GetInteger();
	mTransmissionMode = BitbusTransmissionModeType ( U32 ( mBitbusTransmissionInterface->GetNumber() ) );
	mBitbusAddressingMode = BitbusAddressingMode ( U32 ( mBitbusAddressingModeInterface->GetNumber() ) );

	ClearChannels();
	AddChannel ( mInputChannel, "BITBUS", true );

	return true;
}

void BitbusAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mInputChannelInterface->SetChannel ( mInputChannel );
	mBitRateInterface->SetInteger ( mBitRate );
	mBitbusTransmissionInterface->SetNumber ( mTransmissionMode );
	mBitbusAddressingModeInterface->SetNumber ( mBitbusAddressingMode );
}

void BitbusAnalyzerSettings::LoadSettings ( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString ( settings );

	text_archive >> mInputChannel;
	text_archive >> mBitRate;
	text_archive >> * ( U32* ) &mTransmissionMode;
	text_archive >> * ( U32* ) &mBitbusAddressingMode;

	ClearChannels();
	AddChannel ( mInputChannel, "BITBUS", true );

	UpdateInterfacesFromSettings();
}

const char* BitbusAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive << mInputChannel;
	text_archive << mBitRate;
	text_archive << U32 ( mTransmissionMode );
	text_archive << U32 ( mBitbusAddressingMode );

	return SetReturnString ( text_archive.GetString() );
}
