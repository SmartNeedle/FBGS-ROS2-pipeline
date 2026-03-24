#pragma once

#include <inttypes.h>
#include <string>
#include <iostream>
#include "StreamClient.h"

StreamClient::StreamClient(const char* _address, const char* _port)
{
    // Initialize connection variables
    address = _address;
    port = _port;
    ConnectSocket = INVALID_SOCKET;
    isConnected = NOT_CONNECTED;
    stop = FALSE;

    fiberIndex = 255;
    error = 65000;
    length = 0;
    lineNumber = 0;
    timestamp = 0;

    // initialize data array pointers
    curvature = nullptr;
    angle = nullptr;
    shape = nullptr;
    temperature = nullptr;

    recvbuf = new char[DEFAULT_BUFLEN];

    fieldLength = 0;
    ID = 255;

    // initialize dynamic array lengths
    curvatureArrayLength = 0;
    angleArrayLength = 0;
    shapeArrayLength = 0;
    temperatureArrayLength = 0;
    arrayLength = 0;

    // initialize Cores dynamic arrays
    for (int i = 0; i < 4; i++)
    {
        allCoresData[i].spectrumWL = nullptr;
        allCoresData[i].spectrumPower = nullptr;
        allCoresData[i].peaksWL = nullptr;
        allCoresData[i].peaksPower = nullptr;
    }

    //spectrumWLSize = { 0; 0; 0;0 };
    //spectrumPowerSize = 0;
    //peaksWLSize = 0;
    //peaksPowerSize = 0;
    
}

StreamClient::~StreamClient()
{
    if (isConnected)
        closeStream();

    // Delete all dynamic arrays
    
    if (curvature != nullptr)
        delete curvature;

    if (angle != nullptr)
        delete angle;

    if (shape != nullptr)
        delete shape;

    if (temperature != nullptr)
        delete temperature;

    for (int j = 0; j < 4; j++)
    {
        if (allCoresData[j].spectrumWL != nullptr)
            delete allCoresData[j].spectrumWL;

        if (allCoresData[j].spectrumPower != nullptr)
            delete allCoresData[j].spectrumPower;

        if (allCoresData[j].peaksWL != nullptr)
            delete allCoresData[j].peaksWL;

        if (allCoresData[j].peaksPower != nullptr)
            delete allCoresData[j].peaksPower;
    }
}

int StreamClient::connectStream()
{
    static WSADATA wsaData;
    static struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed with error: " << iResult << "\n";
        return 2;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(address, port, &hints, &result);
    if (iResult != 0) {
        cout << "getaddrinfo failed with error: " << iResult << "\n";
        WSACleanup();
        return 3;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            cout << "socket failed with error: " << WSAGetLastError() << "\n";
            WSACleanup();
            return 4;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        cout << "Unable to connect to server!\n";
        WSACleanup();
        return 5;
    }

    // shutdown the connection since no data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        cout << "shutdown failed with error: " << WSAGetLastError() << "\n";
        closesocket(ConnectSocket);
        WSACleanup();
        return 6;
    }
    else
        isConnected = CONNECTED;
    return 0;
}

int StreamClient::readStream()
{
    // the current implementation uses basic windows (winsock2) TCP communication. In the current implementation, the recv() function is blocking, thus ensuring a whole packet is read.
    // the counterpart is that if ShapeCore closes or connection is lost, then the program blocks
    // An ideal implementation uses an event driven communication, where an event is triggered when the expected number of bytes is read.
    // First read 4 bytes to know the remaining size of the packet, then read the packet
    
 //   iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
    iResult = recv(ConnectSocket, recvbuf, 4, MSG_WAITALL); // read size of the message
    if (iResult != 4)
    {
        return 1; // reading failed
    }
    memcpy(&length, recvbuf, 4);

    if (length <= recvbuflen)
    {
        iResult = recv(ConnectSocket, recvbuf, length, MSG_WAITALL);

        if (iResult == length)
        {
            memcpy(&fiberIndex, &recvbuf[0], 1);
            uint32_t fieldOffset = 1;

            while (fieldOffset < length)
            {
                memcpy(&fieldLength, &recvbuf[fieldOffset], 4);
                fieldOffset += 4;
                memcpy(&ID, &recvbuf[fieldOffset], 2);
                fieldOffset += 2;

                switch (ID)
                {
                default:
                    return 1; // not recognized message ID
                    break;
                case 0: // error
                    memcpy(&error, &recvbuf[fieldOffset], 2);
                    fieldOffset += 2;
                    //cout << "     error : " << error << "\n";
                    break;

                case 1: // line number
                    memcpy(&lineNumber, &recvbuf[fieldOffset], 8);
                    fieldOffset += 8;
                    //cout << "     Line number : " << lineNumber << "\n";
                    break;

                case 2:    // timestamp
                    memcpy(&timestamp, &recvbuf[fieldOffset], 8);
                    //cout << "     Timestamp : " << timestamp << "\n";
                    fieldOffset += 8;
                    break;

                case 3:    // Curvature
                    // copies the number of elements in the array
                    memcpy(&arrayLength, &recvbuf[fieldOffset], 4);
                    fieldOffset += 4;

                    // Allocates/reallocates array if necessary
                    if (curvatureArrayLength != arrayLength)
                    {
                        curvatureArrayLength = arrayLength;
                        if (curvature != nullptr)
                            delete[] curvature;
                        if (arrayLength != 0)
                            curvature = new float_t[arrayLength];
                        else
                            curvature = nullptr;
                    }

                    memcpy(curvature, &recvbuf[fieldOffset], 4 * curvatureArrayLength);
                    fieldOffset += 4 * curvatureArrayLength;

                    //cout << "     Curvature : " << curvature[0] << ", " << curvature[1] << ", " << curvature[2] << ", " << curvature[3] << ", " << curvature[4] << ", " << curvature[5] << ", " << curvature[6] << ", " << curvature[7] << ", " << curvature[8] << ", " << curvature[9] << ", " << curvature[10] << ", " << curvature[11] << ", " << curvature[11] << "\n";
                    break;

                case 4:    // angle
                    // copies the number of elements in the array
                    memcpy(&arrayLength, &recvbuf[fieldOffset], 4);
                    fieldOffset += 4;

                    // Allocates/reallocates array if necessary
                    if (angleArrayLength != arrayLength)
                    {
                        angleArrayLength = arrayLength;
                        if (angle != nullptr)
                            delete[] angle;
                        if (arrayLength != 0)
                            angle = new float_t[arrayLength];
                        else
                            angle = nullptr;
                    }

                    memcpy(angle, &recvbuf[fieldOffset], 4 * angleArrayLength);
                    fieldOffset += 4 * angleArrayLength;

                    cout << "     Angle : " << angle[0] << ", " << angle[1] << ", " << angle[2] << ", " << angle[3] << ", " << angle[4] << ", " << angle[5] << ", " << angle[6] << ", " << angle[7] << ", " << angle[8] << ", " << angle[9] << ", " << angle[10] << ", " << angle[11] << ", " << angle[11] << "\n";
                    break;

                case 5: // Shape
                    // Reads the number of values in the array
                    uint32_t arrayWidth;
                    memcpy(&arrayWidth, &recvbuf[fieldOffset], 4);
                    uint32_t arrayHeight;
                    memcpy(&arrayHeight, &recvbuf[fieldOffset + 4], 4);

                    arrayLength = arrayHeight * arrayWidth;

                    //cout << "ArrayWidth = " << arrayWidth << "\n" << "ArrayHeight = " << arrayHeight << "\n" << "ArrayLength = " << arrayLength << "\n";

                    fieldOffset += 8;

                    // Allocates/reallocates array if necessary
                    if (shapeArrayLength != arrayLength)
                    {
                        shapeArrayLength = arrayLength;
                        if (shape != nullptr)
                            delete[] shape;
                        if (arrayLength != 0)
                            shape = new float_t[arrayLength];
                        else
                            shape = nullptr;
                    }

                    memcpy(shape, &recvbuf[fieldOffset], 4 * shapeArrayLength);
                    fieldOffset += 4 * shapeArrayLength;

                    //if (arrayLength != 0)
                        //cout << "     Shape : \n        " << shape[0] << ", " << shape[1] << ", " << shape[2] << "\n        " << shape[3] << ", " << shape[4] << ", " << shape[5] << "\n        " << shape[6] << ", " << shape[7] << ", " << shape[8] << "\n        " << shape[9] << ", " << shape[10] << ", " << shape[11] << "\n";
                    break;

                case 6: // Temperature
                    // copies the number of elements in the array
                    memcpy(&arrayLength, &recvbuf[fieldOffset], 4);
                    fieldOffset += 4;

                    // Allocates/reallocates array if necessary
                    if (temperatureArrayLength != arrayLength)
                    {
                        temperatureArrayLength = arrayLength;
                        if (temperature != nullptr)
                            delete[] temperature;
                        if (arrayLength != 0)
                            temperature = new float_t[arrayLength];
                        else
                            temperature = nullptr;
                    }

                    memcpy(temperature, &recvbuf[fieldOffset], 4 * temperatureArrayLength);
                    fieldOffset += 4 * temperatureArrayLength;

                    //cout << "     Temperature : " << temperature[0] << ", " << temperature[1] << ", " << temperature[2] << ", " << temperature[3] << ", " << temperature[4] << ", " << temperature[5] << ", " << temperature[6] << ", " << temperature[7] << ", " << temperature[8] << ", " << temperature[9] << ", " << temperature[10] << ", " << temperature[11] << ", " << temperature[11] << "\n";
                    break;

                case 7: // spectra field
                    for (int i = 0; i < 4; i++)
                    {
                        uint32_t spectraFieldLength;
                        memcpy(&spectraFieldLength, &recvbuf[fieldOffset], 4);

                        uint32_t fieldEnd = fieldOffset + spectraFieldLength;
                        fieldOffset += 4;

                        uint8_t channel;
                        memcpy(&channel, &recvbuf[fieldOffset], 1);
                        fieldOffset += 1;

                        //cout << "channel " << i << "\n";

                        while (fieldOffset < fieldEnd) // reads the sub-fields
                        {
                            uint16_t spectraID;
                            memcpy(&fieldLength, &recvbuf[fieldOffset], 4);
                            fieldOffset += 4;

                            memcpy(&spectraID, &recvbuf[fieldOffset], 2);
                            fieldOffset += 2;

                            switch (spectraID)
                            {
                            default:
                                return 2;
                                break;

                            case 0: // Timestamp
                                memcpy(&allCoresData[i].timestamp, &recvbuf[fieldOffset], 8);
                                fieldOffset += 8;
                                //cout << "    Timestamp : " << allCoresData[channel].timestamp << "\n";
                                break;

                            case 1: // Sample Number
                                memcpy(&allCoresData[i].sampleNumber, &recvbuf[fieldOffset], 8);
                                fieldOffset += 8;
                                //cout << "    Sample number : " << allCoresData[channel].sampleNumber << "\n";
                                break;

                            case 2: // Error Status
                                memcpy(&allCoresData[i].errorStatus, &recvbuf[fieldOffset], 2);
                                fieldOffset += 2;
                                //cout << "    Error status : " << allCoresData[channel].errorStatus <<"\n";
                                break;

                            case 3: // Spectrum WL
                                // Reads the number of values in the array
                                memcpy(&arrayLength, &recvbuf[fieldOffset], 4);
                                fieldOffset += 4;

                                // Allocates/reallocates array if necessary
                                if (spectrumWLSize[i] != arrayLength)
                                {
                                    spectrumWLSize[i] = arrayLength;
                                    if (allCoresData[i].spectrumWL != nullptr)
                                        delete[] allCoresData[i].spectrumWL;
                                    if (spectrumWLSize[i] != 0)
                                        allCoresData[i].spectrumWL = new uint32_t[spectrumWLSize[i]];
                                    else
                                        allCoresData[i].spectrumWL = nullptr;
                                }

                                // fills array
                                memcpy(allCoresData[i].spectrumWL, &recvbuf[fieldOffset], spectrumWLSize[i] * 4);
                                fieldOffset += spectrumWLSize[i] * 4;

                                //cout << "    Spectrum Wl : " <<  allCoresData[i].spectrumWL[0] << ", " << allCoresData[i].spectrumWL[1] << ", " << allCoresData[i].spectrumWL[2] << ", " << allCoresData[i].spectrumWL[3] << ", " << allCoresData[i].spectrumWL[4] << ", " << allCoresData[i].spectrumWL[5] << "\n";
                                break;

                            case 4: // Spectrum Powers
                                // Reads the number of values in the array
                                memcpy(&arrayLength, &recvbuf[fieldOffset], 4);
                                fieldOffset += 4;

                                // Allocates/reallocates array if necessary
                                if (spectrumPowerSize[i] != arrayLength)
                                {
                                    spectrumPowerSize[i] = arrayLength;
                                    if (allCoresData[i].spectrumPower != nullptr)
                                        delete[] allCoresData[i].spectrumPower;
                                    if (spectrumPowerSize[i] != 0)
                                        allCoresData[i].spectrumPower = new uint16_t[spectrumPowerSize[i]];
                                    else
                                        allCoresData[i].spectrumPower = nullptr;
                                }

                                // fills array
                                memcpy(allCoresData[i].spectrumPower, &recvbuf[fieldOffset], spectrumPowerSize[i] * 2);
                                fieldOffset += spectrumPowerSize[i] * 2;

                                //cout << "    Spectrum power : " << ", " << allCoresData[i].spectrumPower[0] << ", " << allCoresData[i].spectrumPower[1] << ", " << allCoresData[i].spectrumPower[2] << ", " << allCoresData[i].spectrumPower[3] << ", " << allCoresData[i].spectrumPower[4] << ", " << allCoresData[i].spectrumPower[5] << "\n";
                                break;

                            case 5: // Peaks WL
                                // Reads the number of values in the array
                                memcpy(&arrayLength, &recvbuf[fieldOffset], 4);
                                fieldOffset += 4;

                                // Allocates/reallocates array if necessary
                                if (peaksWLSize[i] != arrayLength)
                                {
                                    peaksWLSize[i] = arrayLength;
                                    if (allCoresData[i].peaksWL != nullptr)
                                        delete[] allCoresData[i].peaksWL;
                                    if (peaksWLSize != 0)
                                        allCoresData[i].peaksWL = new uint32_t[peaksWLSize[i]];
                                    else
                                        allCoresData[i].peaksWL = nullptr;
                                }

                                // fills array
                                memcpy(allCoresData[i].peaksWL, &recvbuf[fieldOffset], peaksWLSize[i] * 4);
                                fieldOffset += peaksWLSize[i] * 4;

                                //cout << "    Peaks WL : " << allCoresData[i].peaksWL[0] << ", " << allCoresData[i].peaksWL[1] << ", " << allCoresData[i].peaksWL[2] << ", " << allCoresData[i].peaksWL[3] << ", " << allCoresData[i].peaksWL[4] << ", " << allCoresData[i].peaksWL[5] << "\n";
                                break;

                            case 6: // Peaks power
                                // Reads the number of values in the array
                                memcpy(&arrayLength, &recvbuf[fieldOffset], 4);
                                fieldOffset += 4;

                                // Allocates/reallocates array if necessary
                                if (peaksPowerSize[i] != arrayLength)
                                {
                                    peaksPowerSize[i] = arrayLength;
                                    if (allCoresData[i].peaksPower != nullptr)
                                        delete[] allCoresData[i].peaksPower;
                                    if (arrayLength != 0)
                                        allCoresData[i].peaksPower = new uint16_t[arrayLength];
                                    else
                                        allCoresData[i].peaksPower = nullptr;
                                }

                                // fills array
                                memcpy(allCoresData[i].peaksPower, &recvbuf[fieldOffset], peaksPowerSize[i] * 2);
                                fieldOffset += peaksPowerSize[i] * 2;

                                //cout << "    Peaks power : " << allCoresData[i].peaksPower[0] << ", " << allCoresData[i].peaksPower[1] << ", " << allCoresData[i].peaksPower[2] << ", " << allCoresData[i].peaksPower[3] << ", " << allCoresData[i].peaksPower[4] << ", " << allCoresData[i].peaksPower[5] << "\n";
                                break;

                            case 7: // Spectrometer Temperature
                                memcpy(&allCoresData[i].spectroTemperature, &recvbuf[fieldOffset], 4);
                                fieldOffset += 4;
                                break;

                            }
                        }
                    }
                    break;
                }
            }
            return 0;
        }
        else if (iResult == 0)
        {
            isConnected = NOT_CONNECTED;
            return 1;
        }
        else
        {
            return WSAGetLastError();
        }
        return 0;
    }
}

int StreamClient::closeStream()
{
    delete[] recvbuf;

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    isConnected = NOT_CONNECTED;

    return 0;
}