#pragma once

#define WIN32_LEAN_AND_MEAN


#include <cmath>
#include <inttypes.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
using namespace std;

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib for TCP communication
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define CONNECTED 1
#define NOT_CONNECTED 0
#define DEFAULT_BUFLEN 100000

class StreamClient
{
private:
	// TCP communication variables
	bool stop;
	SOCKET ConnectSocket;
	const char* address;
	const char* port;
	char* recvbuf;
	int iResult;
	const int recvbuflen = DEFAULT_BUFLEN;

	// Message values
	int32_t fieldLength;
	uint16_t ID;

	uint32_t arrayLength;

public:

	struct coreData
	{
		double_t timestamp;
		uint64_t sampleNumber;
		uint16_t errorStatus;
		uint32_t* spectrumWL;
		uint16_t* spectrumPower;
		uint32_t* peaksWL;
		uint16_t* peaksPower;
		uint32_t spectroTemperature;
	};

	uint32_t spectrumWLSize[4] { 0,0,0,0 }; // number of elements in the spectrum WL array
	uint32_t spectrumPowerSize[4]{ 0,0,0,0 }; // number of elements in the spectrum power array
	uint32_t peaksWLSize[4] { 0,0,0,0 };	// number of elements in the peaks WL array
	uint32_t peaksPowerSize[4] { 0,0,0,0 };	// number of elements in the spectra power array

	coreData allCoresData[4];

	// Message data
	uint32_t length;
	uint8_t fiberIndex;
	uint16_t error;
	uint64_t lineNumber;
	double_t timestamp;
	float* curvature;
	float* angle;
	float* shape;
	float* temperature;

	uint32_t curvatureArrayLength;
	uint32_t angleArrayLength;
	uint32_t shapeArrayLength;
	uint32_t temperatureArrayLength;

	bool isConnected;

	StreamClient(const char* address, const char* port);
	~StreamClient();

	int connectStream();
	int readStream();
	int closeStream();
};

