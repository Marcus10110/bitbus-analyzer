#ifndef BITBUS_ANALYZER_RESULTS
#define BITBUS_ANALYZER_RESULTS

#include <AnalyzerResults.h>
#include <string>

using namespace std;

class BitbusAnalyzer;
class BitbusAnalyzerSettings;

class BitbusAnalyzerResults : public AnalyzerResults
{
public:
	BitbusAnalyzerResults ( BitbusAnalyzer* analyzer, BitbusAnalyzerSettings* settings );
	virtual ~BitbusAnalyzerResults();

	virtual void GenerateBubbleText ( U64 frame_index, Channel& channel, DisplayBase display_base );
	virtual void GenerateExportFile ( const char* file, DisplayBase display_base, U32 export_type_user_id );

	virtual void GenerateFrameTabularText ( U64 frame_index, DisplayBase display_base );
	virtual void GeneratePacketTabularText ( U64 packet_id, DisplayBase display_base );
	virtual void GenerateTransactionTabularText ( U64 transaction_id, DisplayBase display_base );

protected: //functions
	void GenBubbleText ( U64 frame_index, DisplayBase display_base, bool tabular );

	void GenFlagFieldString ( const Frame & frame, bool tabular );
	void GenAddressFieldString ( const Frame & frame, DisplayBase display_base, bool tabular );
	void GenSOHFieldString ( const Frame & frame, DisplayBase display_base, bool tabular );
	void GenInformationFieldString ( const Frame & frame, DisplayBase display_base, bool tabular );
	void GenFcsFieldString ( const Frame & frame, DisplayBase display_base, bool tabular );
	void GenReservedFieldString ( const Frame & frame, DisplayBase display_base, bool tabular );
        void GenAbortFieldString (bool tabular );

	string EscapeByteStr ( const Frame & frame );
	string GenEscapedString ( const Frame & frame );
        string genNumberInfo( const Frame &frame );
protected:  //vars
	BitbusAnalyzerSettings* mSettings;
	BitbusAnalyzer* mAnalyzer;
};

#endif //BITBUS_ANALYZER_RESULTS
