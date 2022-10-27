#include "BitbusAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "BitbusAnalyzer.h"
#include "BitbusAnalyzerSettings.h"
#include <fstream>
#include <sstream>

extern void do_debug(const char *fmt, ...);
#define DBG(x,...)



BitbusAnalyzerResults::BitbusAnalyzerResults ( BitbusAnalyzer* analyzer, BitbusAnalyzerSettings* settings )
	:	AnalyzerResults(),
	    mSettings ( settings ),
	    mAnalyzer ( analyzer )
{
}

BitbusAnalyzerResults::~BitbusAnalyzerResults()
{
}

void BitbusAnalyzerResults::GenerateBubbleText ( U64 frame_index, Channel& /*channel*/, DisplayBase display_base )
{
        DBG("GenerateBubbleText: enter");
        GenBubbleText ( frame_index, display_base, false );
        DBG("GenerateBubbleText: leave");
}

void BitbusAnalyzerResults::GenBubbleText ( U64 frame_index, DisplayBase display_base, bool tabular )
{
        if( !tabular )
                ClearResultStrings();

        DBG("Processing frame");
        Frame frame = GetFrame ( frame_index );

        DBG("Frame type %d", frame.mType );

        switch ( frame.mType )
        {
        case BITBUS_FIELD_FLAG:
                GenFlagFieldString ( frame, tabular );
                break;
        case BITBUS_FIELD_SOH:
                GenSOHFieldString ( frame, display_base, tabular );
                break;
        case BITBUS_FIELD_ADDRESS:
                GenAddressFieldString ( frame, display_base, tabular );
                break;
        case BITBUS_FIELD_INFORMATION:
                GenInformationFieldString ( frame, display_base, tabular );
                break;
        case BITBUS_FIELD_FCS:
                GenFcsFieldString ( frame, display_base, tabular );
                break;
        case BITBUS_ABORT_SEQ:
                GenAbortFieldString ( tabular );
                break;
        case BITBUS_FIELD_RESERVED:
                GenReservedFieldString ( frame, display_base, tabular );
                break;

        }
}

void BitbusAnalyzerResults::GenFlagFieldString ( const Frame & frame, bool tabular )
{
        char* flagTypeStr=0;
        switch ( frame.mData1 )
        {
        case BITBUS_FLAG_START:
                flagTypeStr = "Start";
                break;
        case BITBUS_FLAG_END:
                flagTypeStr = "End";
                break;
        case BITBUS_FLAG_FILL:
                flagTypeStr = "Fill";
                break;
        default:
                flagTypeStr = "Invalid";
                break;
        }

        if ( !tabular )
        {
                AddResultString ( "F" );
                AddResultString ( "FL" );
                AddResultString ( "FLAG" );
                AddResultString ( flagTypeStr, " FLAG" );
                AddResultString ( flagTypeStr, " Flag Delimiter" );
        } else {
                AddTabularText( flagTypeStr, " Flag Delimiter" );
        }
}

void BitbusAnalyzerResults::GenAbortFieldString ( bool tabular )
{
        char* seq = 0;
        if ( mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BIT_SYNC )
        {
                seq = "(>=7 1-bits)";
        }
        else
        {
                seq = "(0x7D-0x7F)";
        }

        if ( !tabular )
        {
                AddResultString ( "AB!" );
                AddResultString ( "ABORT!" );
                AddResultString ( "ABORT SEQUENCE!", seq );
        }
        else
        {
                AddTabularText( "ABORT SEQUENCE!", seq );
        }
}



string BitbusAnalyzerResults::GenEscapedString ( const Frame & frame )
{
	stringstream ss;
	if ( frame.mFlags & BITBUS_ESCAPED_BYTE )
	{
		char dataStr[ 32 ];
		AnalyzerHelpers::GetNumberString ( frame.mData1, Hexadecimal, 8, dataStr, 32 );
		char dataInvStr[ 32 ];
		AnalyzerHelpers::GetNumberString ( BitbusAnalyzerSettings::Bit5Inv ( frame.mData1 ), Hexadecimal, 8, dataInvStr, 32 );

		ss << " - ESCAPED: 0x7D-" << dataStr << "=" << dataInvStr;
	}

	return ss.str();

}

std::string BitbusAnalyzerResults::genNumberInfo( const Frame &frame )
{
        std::string ret;
        char decimal[ 64 ];
	char hex[ 64 ];
        AnalyzerHelpers::GetNumberString ( frame.mData1, Decimal, 8, decimal, 64 );
        AnalyzerHelpers::GetNumberString ( frame.mData1, Hexadecimal,8, hex, 64 );
        ret = decimal;
        ret += " [";
        ret += hex;
        ret += "]";
        return ret;
}


void BitbusAnalyzerResults::GenAddressFieldString ( const Frame & frame, DisplayBase display_base, bool tabular )
{
        std::string addrStr = genNumberInfo(frame);
        std::string escStr = GenEscapedString ( frame );

        if ( !tabular )
        {
                AddResultString ( "A" );
                AddResultString ( "AD" );
                AddResultString ( "ADDR" );
                AddResultString ( "ADDR ", addrStr.c_str(), escStr.c_str() );
                AddResultString ( "Address ", addrStr.c_str(), escStr.c_str() );
        }
        else {
                AddTabularText( "Address ", addrStr.c_str(), escStr.c_str() );
        }
}

void BitbusAnalyzerResults::GenReservedFieldString ( const Frame & frame, DisplayBase display_base, bool tabular )
{
        std::string rsvdStr = genNumberInfo(frame);
        string escStr = GenEscapedString ( frame );

        if ( !tabular )
        {
                AddResultString ( "R" );
                AddResultString ( "RS" );
                AddResultString ( "RSV" );
                AddResultString ( "RSVD ", rsvdStr.c_str(), escStr.c_str() );
                AddResultString ( "Reserved ", rsvdStr.c_str(), escStr.c_str() );
        }
        else {
                AddTabularText( "Reserved ", rsvdStr.c_str(), escStr.c_str() );
        }
}


void BitbusAnalyzerResults::GenSOHFieldString ( const Frame & frame, DisplayBase display_base, bool tabular )
{
        std::string sohStr = genNumberInfo(frame);

        string escStr = GenEscapedString ( frame );

        if ( !tabular )
        {
                AddResultString ( "S" );
                AddResultString ( "SO" );
                AddResultString ( "SOH" );
                AddResultString ( "SOH ", sohStr.c_str(), escStr.c_str() );
                AddResultString ( "SOH ", sohStr.c_str(), escStr.c_str() );
        }
        else {
                AddTabularText( "Start Of Header ", sohStr.c_str(), escStr.c_str() );
        }
}

void BitbusAnalyzerResults::GenInformationFieldString ( const Frame & frame, const DisplayBase display_base, bool tabular )
{
        std::string informationStr = genNumberInfo(frame);
	char numberStr[ 64 ];
	AnalyzerHelpers::GetNumberString ( frame.mData2, Decimal, 32, numberStr, 64 );

	string escStr = GenEscapedString ( frame );

	if ( !tabular )
	{
		AddResultString ( "I" );
		AddResultString ( "I ", numberStr );
		AddResultString ( "I ", numberStr, " (", informationStr.c_str() ,")", escStr.c_str() );
                AddResultString ( "Info ", numberStr, " (", informationStr.c_str() ,")", escStr.c_str() );
        }
        else {
                AddTabularText("Info ", numberStr, " (", informationStr.c_str() ,")", escStr.c_str() );
        }
}

void BitbusAnalyzerResults::GenFcsFieldString ( const Frame & frame, DisplayBase display_base, bool tabular )
{
        U32 fcsBits = 16;
        char *crcTypeStr= "16";

	char readFcsStr[ 128 ];
	AnalyzerHelpers::GetNumberString ( frame.mData1, display_base, fcsBits, readFcsStr, 128 );
	char calcFcsStr[ 128 ];
	AnalyzerHelpers::GetNumberString ( frame.mData2, display_base, fcsBits, calcFcsStr, 128 );

	stringstream fieldNameStr;
	if ( frame.mFlags & DISPLAY_AS_ERROR_FLAG )
	{
		fieldNameStr << "!";
	}

	fieldNameStr << "FCS CRC" << crcTypeStr;

	if ( !tabular )
	{
		AddResultString ( "CRC" );
		AddResultString ( fieldNameStr.str().c_str() );
	}

	if ( frame.mFlags & DISPLAY_AS_ERROR_FLAG )
	{
		fieldNameStr << " ERROR";
	}
	else
	{
		fieldNameStr << " OK";
	}

	if ( !tabular )
	{
		AddResultString ( fieldNameStr.str().c_str() );
	}

	if ( frame.mFlags & DISPLAY_AS_ERROR_FLAG )
	{
		fieldNameStr << " - CALC CRC[" << calcFcsStr << "] != READ CRC[" << readFcsStr << "]";
	}

    if( !tabular )
        AddResultString ( fieldNameStr.str().c_str()  );
    else
        AddTabularText( fieldNameStr.str().c_str()  );

}

string BitbusAnalyzerResults::EscapeByteStr ( const Frame & frame )
{
	if ( ( mSettings->mTransmissionMode == BITBUS_TRANSMISSION_BYTE_ASYNC ) && ( frame.mFlags & BITBUS_ESCAPED_BYTE ) )
	{
		return string ( "0x7D-" );
	}
	else
	{
		return string ( "" );
	}
}

void BitbusAnalyzerResults::GenerateExportFile ( const char* file, DisplayBase display_base, U32 /*export_type_user_id*/ )
{
	ofstream fileStream ( file, ios::out );

	U64 triggerSample = mAnalyzer->GetTriggerSample();
	U32 sampleRate = mAnalyzer->GetSampleRate();

	const char* sepChar = " ";

	U8 fcsBits=16;

	fileStream << "Time[s],Address,Information,FCS" << endl;

	char escapeStr[ 5 ];
	AnalyzerHelpers::GetNumberString ( BITBUS_ESCAPE_SEQ_VALUE, display_base, 8, escapeStr, 5 );

	U64 numFrames = GetNumFrames();
	U64 frameNumber = 0;

	if ( numFrames==0 )
	{
		UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames );
		return;
	}

	for ( ; ; )
	{
		bool doAbortFrame = false;
		// Re-sync to start reading BITBUS frames from the Address Byte
		Frame firstAddressFrame;
		for ( ; ; )
		{
			firstAddressFrame = GetFrame ( frameNumber );

			// Check for abort
			if ( firstAddressFrame.mType == BITBUS_FIELD_SOH ||
			        firstAddressFrame.mType == BITBUS_FIELD_ADDRESS ) // It's and address frame
			{
				break;
			}
			else
			{
				frameNumber++;
				if ( frameNumber >= numFrames )
				{
					UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames );
					return;
				}
			}

		}

		// 1)  Time [s]
		char timeStr[ 64 ];
		AnalyzerHelpers::GetTimeString ( firstAddressFrame.mStartingSampleInclusive, triggerSample, sampleRate, timeStr, 64 );
		fileStream << timeStr << ",";

		// 2) Address Field
		if ( mSettings->mBitbusAddressingMode == BITBUS_ADDRESS_SOF )
		{
			firstAddressFrame = GetFrame ( frameNumber );
			if ( firstAddressFrame.mType != BITBUS_ADDRESS_EXTENDED )
			{
				fileStream << "," << endl;
				continue;
			}

			char addressStr[ 64 ];
			AnalyzerHelpers::GetNumberString ( firstAddressFrame.mData1, display_base, 8, addressStr, 64 );
			fileStream << EscapeByteStr ( firstAddressFrame ) << addressStr << ",";
		}
		else // Check for extended address
		{
			Frame nextAddress = firstAddressFrame;
			for ( ; ; )
			{

				if ( nextAddress.mType != BITBUS_ADDRESS_EXTENDED ) // ERROR
				{
					fileStream << "," << endl;
					break;
				}

				bool endOfAddress = ( ( nextAddress.mData1 & 0x01 ) == 0 );

				char addressStr[ 64 ];
				AnalyzerHelpers::GetNumberString ( nextAddress.mData1, display_base, 8, addressStr, 64 );
				string sep = ( endOfAddress && nextAddress.mData2 == 0 ) ? string() : string ( sepChar );
				fileStream  << sep << EscapeByteStr ( nextAddress ) << addressStr;

				if ( endOfAddress ) // no more bytes of address?
				{
					fileStream << ",";
					break;
				}
				else
				{
					frameNumber++;
					if ( frameNumber >= numFrames )
					{
						UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames );
						return;
					}
					nextAddress = GetFrame ( frameNumber );
				}
			}

		}

		fileStream << ",";

		frameNumber++;
		if ( frameNumber >= numFrames )
		{
			UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames );
			return;
		}

		// 5) Information Fields
		for ( ; ; )
		{
			Frame infoFrame = GetFrame ( frameNumber );

			// Check for flag
			if ( infoFrame.mType == BITBUS_FIELD_FLAG )
			{
				fileStream << ",";
				break;
			}

			// Check for info byte
			if ( infoFrame.mType == BITBUS_FIELD_INFORMATION ) // ERROR
			{
				char infoByteStr[ 64 ];
				AnalyzerHelpers::GetNumberString ( infoFrame.mData1, display_base, 8, infoByteStr, 64 );
				fileStream << sepChar << EscapeByteStr ( infoFrame ) << infoByteStr;
				frameNumber++;
				if ( frameNumber >= numFrames )
				{
					UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames );
					return;
				}
			}
			else
			{
				fileStream << ",";
				break;
			}
		}

		if ( doAbortFrame )
		{
			continue;
		}

		// 6) FCS Field
		Frame fcsFrame = GetFrame ( frameNumber );
		if ( fcsFrame.mType != BITBUS_FIELD_FCS )
		{
			fileStream << "," << endl;
		}
		else // BITBUS_FIELD_FCS Frame
		{
			char fcsStr[ 128 ];
			AnalyzerHelpers::GetNumberString ( fcsFrame.mData1, display_base, fcsBits, fcsStr, 128 );
			fileStream << fcsStr << endl;
		}

		frameNumber++;
		if ( frameNumber >= numFrames )
		{
			UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames );
			return;
		}

		if ( UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames ) )
		{
			return;
		}
	}

	UpdateExportProgressAndCheckForCancel ( frameNumber, numFrames );


}

void BitbusAnalyzerResults::GenerateFrameTabularText ( U64 frame_index, DisplayBase display_base )
{
        DBG("GenerateFrameTabularText: enter");
        ClearTabularText();
	GenBubbleText ( frame_index, display_base, true );
        DBG("GenerateFrameTabularText: leave");
}

void BitbusAnalyzerResults::GeneratePacketTabularText ( U64 packet_id, DisplayBase display_base )
{
        DBG("GeneratePacketTabularText: enter");

	ClearResultStrings();
        AddResultString ( "not supported" );
        DBG("GenerateFrameTabularText: leave");

}

void BitbusAnalyzerResults::GenerateTransactionTabularText ( U64 transaction_id, DisplayBase display_base )
{
        DBG("GenerateTransactionTabularText: enter");

	ClearResultStrings();
        AddResultString ( "not supported" );
        DBG("GenerateFrameTabularText: leave");

}
