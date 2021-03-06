#include "SkyPortalWiFi.h"

// Constructor for SkyPortalWiFi
SkyPortalWiFi::SkyPortalWiFi()
{

	m_bIsConnected = false;
    m_bIsSynced = false;
    m_bDebugLog = true;

	m_bIsMountEquatorial = false;

    m_bJNOW = false;

    m_b24h = false;
    m_bDdMmYy = false;
    m_bTimeSetOnce = false;
    m_bLimitCached = false;

    m_bIsTracking = false;
    m_bSiderealOn = false;
    m_bGotoFast = false;
    m_bSlewing = false;
    
    m_nPrevAltSteps = 0;
    m_nPrevAzSteps = 0;

    m_dCurrentRa = 0;
    m_dCurrentDec = 0;

    m_dHoursEast = 0;
    m_dHoursWest = 0;
        
    m_dTrackRaArcSecPerMin = 0;
    m_dTrackDecArcSecPerMin = 0;
    
#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\SkyPortalWiFiLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SkyPortalWiFiLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SkyPortalWiFiLog.txt";
#endif
	Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [SkyPortalWiFi] New Constructor Called\n", timestamp);
    fflush(Logfile);
#endif

}


SkyPortalWiFi::~SkyPortalWiFi(void)
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [SkyPortalWiFi] Destructor Called\n", timestamp );
    fflush(Logfile);
#endif
#ifdef PLUGIN_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int SkyPortalWiFi::Connect(char *pszPort)
{
    int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [SkyPortalWiFi::Connect] Called with port %s\n", timestamp, pszPort);
    fflush(Logfile);
#endif

    // TCP port 2000
    nErr = m_pSerx->open(pszPort);
    if(nErr) {
        m_bIsConnected = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::Connect] open failed on port %s : %d\n", timestamp, pszPort, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    m_bIsConnected = true;

    m_bJNOW = true;
    //  get firware version.
    nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
    if(nErr)
        nErr = ERR_COMMNOLINK;

    setTrackingRatesSteps(0, 0, 3); // stop all for now as we're not synced
    return SB_OK;
}


int SkyPortalWiFi::Disconnect(void)
{
    int nErr = SB_OK;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [SkyPortalWiFi::Disconnect] Called\n", timestamp);
    fflush(Logfile);
#endif

    nErr = setTrackingRatesSteps(0, 0, 3);
    nErr |= moveAlt(0);
    nErr |= moveAz(0);
    
    if (m_bIsConnected) {
        if(m_pSerx){
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::Disconnect] closing serial port\n", timestamp);
            fflush(Logfile);
#endif
            m_pSerx->flushTx();
            m_pSerx->purgeTxRx();
            m_pSerx->close();
        }
    }
	m_bIsConnected = false;
    m_bLimitCached = false;

    m_bIsSynced = false;

    m_bJNOW = false;

    m_b24h = false;
    m_bDdMmYy = false;
    m_bTimeSetOnce = false;
    m_bLimitCached = false;

    m_bIsTracking = false;
    m_bSiderealOn = false;
    m_bGotoFast = false;
    m_bSlewing = false;
    
    m_nPrevAltSteps = 0;
    m_nPrevAzSteps = 0;

    m_dCurrentRa = 0;
    m_dCurrentDec = 0;

    m_dHoursEast = 0;
    m_dHoursWest = 0;
        
    m_dTrackRaArcSecPerMin = 0;
    m_dTrackDecArcSecPerMin = 0;

	return nErr;
}


int SkyPortalWiFi::getNbSlewRates()
{
    return SkyPortalWiFi_NB_SLEW_SPEEDS;
}

int SkyPortalWiFi::getRateName(int nZeroBasedIndex, char *pszOut, unsigned int nOutMaxSize)
{
    if (nZeroBasedIndex > SkyPortalWiFi_NB_SLEW_SPEEDS)
        return PLUGIN_ERROR;

    strncpy(pszOut, m_aszSlewRateNames[nZeroBasedIndex], nOutMaxSize);

    return PLUGIN_OK;
}

#pragma mark - SkyPortalWiFi communication

int SkyPortalWiFi::SendCommand(const Buffer_t Cmd, Buffer_t &Resp, const bool bExpectResponse)
{
	int nErr = PLUGIN_OK;
	unsigned long  ulBytesWrite;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	unsigned char szHexMessage[LOG_BUFFER_SIZE];
#endif
	int timeout = 0;
	int nRespLen;
	uint8_t nTarget;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	m_pSerx->purgeTxRx();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(Cmd.data(), szHexMessage, int(Cmd[1]+3), LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] Sending %s\n", timestamp, szHexMessage);
	fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] packet size is %d\n", timestamp, (int)(Cmd[1])+3);
	fflush(Logfile);
#endif
	nErr = m_pSerx->writeFile((void *)Cmd.data(), (unsigned long)(Cmd[1])+3, ulBytesWrite);
	m_pSerx->flushTx();

	if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] m_pSerx->writeFile Error %d\n", timestamp, nErr);
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] m_pSerx->writeFile ulBytesWrite : %lu\n", timestamp, ulBytesWrite);
		fflush(Logfile);
#endif
		return nErr;
	}

	if(bExpectResponse) {
		// Read responses until one is for us or we reach a timeout ?
		do {
			//we're waiting for the answer
			if(timeout>50) {
				return ERR_CMDFAILED;
			}
			// read response
			nErr = ReadResponse(Resp, nTarget, nRespLen);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
			if(nTarget == PC) { // filter out command echo
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				hexdump(Resp.data(), szHexMessage, int(Resp.size()), LOG_BUFFER_SIZE);
				fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response \"%s\"\n", timestamp, szHexMessage);
				fflush(Logfile);
			}
#endif
			if(nErr)
				return nErr;
			m_pSleeper->sleep(100);
			timeout++;
		} while(nTarget != PC);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(Resp.data(), szHexMessage, int(Resp.size()), LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response copied to pszResult : \"%s\"\n", timestamp, szHexMessage);
		fflush(Logfile);
#endif
	}
	return nErr;
}


int SkyPortalWiFi::ReadResponse(Buffer_t &RespBuffer, uint8_t &nTarget, int &nLen)
{
	int nErr = PLUGIN_OK;
	unsigned long ulBytesRead = 0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	unsigned char cHexMessage[LOG_BUFFER_SIZE];
#endif
	uint8_t cChecksum;
	uint8_t cRespChecksum;

	unsigned char pszRespBuffer[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	RespBuffer.clear();
	memset(pszRespBuffer, 0, (size_t) SERIAL_BUFFER_SIZE);

	// Look for a SOM starting character, until timeout occurs
	while (*pszRespBuffer != SOM && nErr == PLUGIN_OK) {
		nErr = m_pSerx->readFile(pszRespBuffer, 1, ulBytesRead, MAX_TIMEOUT);
		if (ulBytesRead !=1) // timeout
			nErr = COMMAND_FAILED;
	}

	if(*pszRespBuffer != SOM || nErr != PLUGIN_OK)
		return ERR_CMDFAILED;

	// Read message length
	nErr = m_pSerx->readFile(pszRespBuffer + 1, 1, ulBytesRead, MAX_TIMEOUT);
	if (nErr != PLUGIN_OK || ulBytesRead!=1)
		return ERR_CMDFAILED;

	nLen = int(pszRespBuffer[1]);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nLen = %d\n", timestamp, nLen);
	fflush(Logfile);
#endif

	// Read the rest of the message
	nErr = m_pSerx->readFile(pszRespBuffer + 2, nLen + 1, ulBytesRead, MAX_TIMEOUT); // the +1 on nLen is to also read the checksum
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pszRespBuffer, cHexMessage, nLen + 3, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] pszRespBuffer = %s\n", timestamp, cHexMessage);
	fflush(Logfile);
#endif

	if(nErr || (ulBytesRead != (nLen+1))) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] error not enough byte in the response\n", timestamp);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Calculating response checksum\n", timestamp);
	fflush(Logfile);
#endif
	// verify checksum
	cChecksum = checksum(pszRespBuffer);
	cRespChecksum = uint8_t(*(pszRespBuffer+nLen+2));

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Calculated checksum is 0x%02X, message checksum is 0x%02X\n", timestamp, cChecksum, cRespChecksum);
	fflush(Logfile);
#endif
	if (cChecksum != cRespChecksum) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse]  if (cChecksum != cRespChecksum) failed  !! WTF !!!  0x%02X, 0x%02X\n", timestamp, cChecksum, cRespChecksum);
		fflush(Logfile);
#endif
		nErr = ERR_CMDFAILED;
	}
	nLen = int(pszRespBuffer[MSG_LEN]-3); // SRC DST CMD [data]
	nTarget = pszRespBuffer[DST_DEV];

	RespBuffer.assign(pszRespBuffer+2+3, pszRespBuffer+2+3+nLen); // just the data without SOM, LEN , SRC, DEST, CMD_ID and checksum
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nLen = %d\n", timestamp, nLen);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nTarget = 0x%02X\n", timestamp, nTarget);
	hexdump(RespBuffer.data(), cHexMessage, int(RespBuffer.size()), LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Resp data =  %s\n", timestamp, cHexMessage);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nErr =  %d\n", timestamp, nErr);
	fflush(Logfile);
#endif
	return nErr;
}



unsigned char SkyPortalWiFi::checksum(const unsigned char *cMessage)
{
	int nIdx;
	char cChecksum = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum(const unsigned char *cMessage)]\n", timestamp);
	fflush(Logfile);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1] = %d\n", timestamp, int(cMessage[1]));
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1]+2 = %d\n", timestamp, int(cMessage[1])+2);
	fflush(Logfile);
#endif

	for (nIdx = 1; nIdx < int(cMessage[1])+2; nIdx++) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::checksum] nIdx = %d  cMessage[nIdx] = '0x%02X'\n", timestamp, nIdx, int(cMessage[nIdx]));
		fflush(Logfile);
#endif
		cChecksum += cMessage[nIdx];
	}
	return (unsigned char)(-cChecksum & 0xff);
}

uint8_t SkyPortalWiFi::checksum(const Buffer_t cMessage)
{
	int nIdx;
	char cChecksum = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum(const Buffer_t cMessage)]\n", timestamp);
	fflush(Logfile);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1] = %d\n", timestamp, int(cMessage[1]));
	fprintf(Logfile, "[%s] [CCelestronFocus::checksum] cMessage[1]+2 = %d\n", timestamp, int(cMessage[1])+2);
	fflush(Logfile);
#endif

	for (nIdx = 1; nIdx < int(cMessage[1])+2; nIdx++) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::checksum] nIdx = %d  cMessage[nIdx] = '0x%02X'\n", timestamp, nIdx, int(cMessage[nIdx]));
		fflush(Logfile);
#endif
		cChecksum += cMessage[nIdx];
	}
	return (uint8_t)(-cChecksum & 0xff);
}

void SkyPortalWiFi::hexdump(const unsigned char* pszInputBuffer, unsigned char *pszOutputBuffer, int nInputBufferSize, int nOutpuBufferSize)
{
	unsigned char *pszBuf = pszOutputBuffer;
	int nIdx=0;

	memset(pszOutputBuffer, 0, nOutpuBufferSize);
	for(nIdx=0; nIdx < nInputBufferSize && pszBuf < (pszOutputBuffer + nOutpuBufferSize -3); nIdx++){
		snprintf((char *)pszBuf,4,"%02X ", pszInputBuffer[nIdx]);
		pszBuf+=3;
	}
}


#pragma mark - mount controller informations

int SkyPortalWiFi::getFirmwareVersion(char *pszVersion, unsigned int nStrMaxLen)
{
    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;
    char AMZVersion[SERIAL_BUFFER_SIZE];
    char ALTVersion[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] SkyPortalWiFi::getFirmwareVersion Getting AZM version\n", timestamp);
    fflush(Logfile);
#endif

    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = AZM;
    Cmd[CMD_ID] = MC_GET_VER;    //get firmware version
    Cmd[5] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] Error getting AZM version : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    snprintf(AMZVersion, nStrMaxLen,"%d.%d", Resp[0] , Resp[1]);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] AZM version : %s\n", timestamp, AMZVersion);
    fflush(Logfile);
#endif

	


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] Getting ALT version\n", timestamp);
    fflush(Logfile);
#endif

    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = ALT;
    Cmd[CMD_ID] = MC_GET_VER;    //get firmware version
    Cmd[5] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] Error getting AZM version : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    snprintf(ALTVersion, nStrMaxLen,"%d.%d", Resp[0] , Resp[1]);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] ALT verrsion : %s\n", timestamp, ALTVersion);
    fflush(Logfile);
#endif

    snprintf(pszVersion,nStrMaxLen, "%s %s",  AMZVersion, ALTVersion);
    return nErr;
}


#pragma mark - Mount Coordinates

int SkyPortalWiFi::getHaAndDec(double &dHa, double &dDec)
{
    int nErr = PLUGIN_OK;

    double dAlt = 0.0, dAz = 0.0;
    double dNewAlt, dNewAz;         // position where we should be in Deg.
    int nAltSteps,nAzSteps;         // current mount position in steps
    int nNewAltSteps,nNewAzSteps;   // position where we should be in steps
    int nAltError, nAzError;        // position error
    int nAltRate, nAzRate;          // guide rate
    float fSecondsDelta;
    double dRa;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec]\n", timestamp);
    fflush(Logfile);
#endif

    nErr = getPosition(nAzSteps, nAltSteps);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Error getting position : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(m_bIsMountEquatorial) {
		dHa = stepToHa(nAzSteps);
		altStepsToDeg(nAltSteps, dDec);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] dHa : %3.2f\n", timestamp, dHa);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Dec : %3.2f\n", timestamp, dDec);
        fflush(Logfile);
#endif
        return nErr;
    }

    // Az Mount.
	azStepsToDeg(nAzSteps, dAz);
	altStepsToDeg(nAltSteps, dAlt);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] dAz : %3.2f\n", timestamp, dAz);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] dAlt : %3.2f\n", timestamp, dAlt);
    fflush(Logfile);
#endif

    m_pTSX->HzToEq(dAz, dAlt, dRa, dDec); // convert  Az,Alt to TSX Ra,Dec
    dHa = m_pTSX->hourAngle(dRa);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Ra : %3.2f\n", timestamp, dRa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Ha : %3.2f\n", timestamp, dHa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Dec : %3.2f\n", timestamp, dDec);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] m_dCurrentRa : %3.2f\n", timestamp, m_dCurrentRa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] m_dCurrentDec : %3.2f\n", timestamp, m_dCurrentDec);
    fflush(Logfile);
#endif

    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] m_bIsTracking : %s\n", timestamp, m_bIsTracking?"True":"False");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] m_bIsSynced : %s\n", timestamp, m_bIsSynced?"True":"False");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] m_mountType : %d\n", timestamp, m_mountType);
    fflush(Logfile);
#endif

    
    // now deal with Az/Alt mount as we need to adjust the tracking rate all the time
    if(m_bIsTracking && m_bIsSynced && m_mountType==MountTypeInterface::AltAz) {
        // adjust tracking rate every RATE_CORRECT_INTERVAL seconds at most
        fSecondsDelta = m_trakingTimer.GetElapsedSeconds();
        if( fSecondsDelta<= RATE_CORRECT_INTERVAL) {
            return nErr;
        }
        m_trakingTimer.Reset();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] adjust the tracking rate\n", timestamp);
        fflush(Logfile);
#endif

        // It should be pointing to the latest goto/move
        // get Alt/Az for where it should be pointing
        m_pTSX->EqToHz(m_dCurrentRa, m_dCurrentDec, dNewAz, dNewAlt);
        // conver to steps
        azDegToSteps(dNewAz, nNewAzSteps);
        altDegToSteps(dNewAlt, nNewAltSteps);

        // compute error
        nAzError = nNewAzSteps - nAzSteps;
        nAltError = nNewAltSteps - nAltSteps;

        // difference between where were and now
        // traking rates are in steps per minute and we check every 30 seconds or so ( actual is fSecondsDelta),
        // so we multiply the differece by 60/fSecondsDelta
        nAzRate = (nNewAzSteps - m_nPrevAzSteps + nAzError) * (60/fSecondsDelta);
        nAltRate = (nNewAltSteps - m_nPrevAltSteps + nAltError)  * (60/fSecondsDelta);

        // update the values for next time around
        m_nPrevAzSteps = nNewAzSteps;
        m_nPrevAltSteps = nNewAltSteps;

        m_dTrackRaArcSecPerMin = (nAzRate / STEPS_PER_DEGREE) * 3600;
        m_dTrackDecArcSecPerMin = (nAltRate / STEPS_PER_DEGREE) * 3600;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Az rate                                 : %d\n", timestamp, nAzRate);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Alt rate                                : %d\n", timestamp, nAltRate);
        fflush(Logfile);
#endif

        // Track function needs rates in 1000*arcmin/minute
        nAzRate  = TRACK_SCALE * nAzRate;
        nAltRate = TRACK_SCALE * nAltRate;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] fSecondsDelta        : %3.2f\n", timestamp, fSecondsDelta);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope prev Az steps (m_nPrevAzSteps)    : %d\n", timestamp, m_nPrevAzSteps);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope prev Alt steps (m_nPrevAltSteps)  : %d\n", timestamp, m_nPrevAltSteps);

        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Az (dAz)                          : %3.2f\n", timestamp, dAz);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Az steps (nAzSteps)               : %d\n", timestamp, nAzSteps);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Alt (dAlt)                        : %3.2f\n", timestamp, dAlt);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Alt steps (nAltSteps)             : %d\n", timestamp, nAltSteps);

        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Target Az (dNewAz)                      : %3.2f\n", timestamp, dNewAz);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Targe Az steps (nNewAzSteps)            : %d\n", timestamp, nNewAzSteps);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Target Alt (dNewAlt)                    : %3.2f\n", timestamp, dNewAlt);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Taret Alt steps (nNewAltSteps)          : %d\n", timestamp, nNewAltSteps);

        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Az error (nAzError)                     : %d\n", timestamp, nAzError);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Alt error (nAltError)                   : %d\n", timestamp, nAltError);

        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Az rate (nAzRate * TRACK_SCALE)         : %d\n", timestamp, nAzRate);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Alt rate (nAltRate * TRACK_SCALE)       : %d\n", timestamp, nAltRate);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Ra rate (m_dTrackRaArcSecPerMin)        : %3.2f\n", timestamp, m_dTrackRaArcSecPerMin);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Dec rate (m_dTrackDecArcSecPerMin)      : %3.2f\n", timestamp, m_dTrackDecArcSecPerMin);

        fflush(Logfile);
#endif

        // update the tracking rate
        nErr = setTrackingRatesSteps(nAzRate, nAltRate, 3);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] tracking rates set\n", timestamp);
        fflush(Logfile);
#endif
    }
    return nErr;
}


int SkyPortalWiFi::getPosition(int &nAzSteps, int &nAltSteps)
{
    int nErr = PLUGIN_OK;
	Buffer_t Cmd(SERIAL_BUFFER_SIZE);
	Buffer_t Resp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSkyPortalWiFi::getPosition]\n", timestamp);
    fflush(Logfile);
#endif


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSkyPortalWiFi::getPosition] Get Az\n", timestamp);
    fflush(Logfile);
#endif
    
    // get AZM
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = AZM;
    Cmd[CMD_ID] = MC_GET_POSITION;
    Cmd[5] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getPosition] error getting nAzSteps : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    // convert counters to proper degrees
    nAzSteps = Resp[0]<<16 | Resp[1]<<8 | Resp[2];

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSkyPortalWiFi::getPosition] Get Alt\n", timestamp);
    fflush(Logfile);
#endif
	
    // get ALT
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = ALT;
    Cmd[CMD_ID] = MC_GET_POSITION;
    Cmd[5] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getPosition] error getting nAltSteps : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    // convert counters to proper degrees
    nAltSteps = Resp[0]<<16 | Resp[1]<<8 | Resp[2];

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getPosition] nAzSteps : %d\n", timestamp, nAzSteps);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getPosition] nAltSteps : %d\n", timestamp, nAltSteps);
    fflush(Logfile);
#endif

    return nErr;
}

int SkyPortalWiFi::setPosition(int nAzSteps, int nAltSteps )
{

    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition]\n", timestamp);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] nAzSteps : %d\n", timestamp, nAzSteps);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] nAltSteps : %d\n", timestamp, nAltSteps);
    fflush(Logfile);
#endif


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] Setting Az to %d ( %06X )\n", timestamp, nAzSteps, nAzSteps);
    fflush(Logfile);
#endif
    // set AZM
    
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 6;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = AZM;
    Cmd[CMD_ID] = MC_SET_POSITION;
    Cmd[5] = (unsigned char) ((nAzSteps & 0x00ff0000) >> 16);
    Cmd[6] = (unsigned char) ((nAzSteps & 0x0000ff00) >> 8);
    Cmd[7] = (unsigned char)  (nAzSteps & 0x000000ff);
    Cmd[8] = checksum(Cmd);
    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] error setting nAzSteps : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] Setting Alt to %d ( %06X )\n", timestamp, nAltSteps, nAltSteps);
    fflush(Logfile);
#endif
    // set ALT
    
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 6;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = ALT;
    Cmd[CMD_ID] = MC_SET_POSITION;
    Cmd[5] = (unsigned char) ((nAltSteps & 0x00ff0000) >> 16);
    Cmd[6] = (unsigned char) ((nAltSteps & 0x0000ff00) >> 8);
    Cmd[7] = (unsigned char)  (nAltSteps & 0x000000ff);
    Cmd[8] = checksum(Cmd);
    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] error setting nAltSteps : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    return nErr;
}

 void SkyPortalWiFi::setMountMode(MountTypeInterface::Type mountType)
{
	m_mountType = mountType;
	m_bIsMountEquatorial = (m_mountType == MountTypeInterface::Symmetrical_Equatorial || m_mountType == MountTypeInterface::Asymmetrical_Equatorial);
}

MountTypeInterface::Type SkyPortalWiFi::mountType()
{
	return m_mountType;
}


#pragma mark - Sync and Cal
int SkyPortalWiFi::syncTo(double dRa, double dDec)
{
    int nErr = PLUGIN_OK;
    double dAltPos, dAzPos;
    int nAltPos, nAzPos;
    double dHa;
    
    m_bIsSynced = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo]\n", timestamp);
    fflush(Logfile);
#endif
    dHa = m_pTSX->hourAngle(dRa);
    
    if(m_bIsMountEquatorial) {
        dAzPos = haToSteps(dHa);
        dAltPos = dDec;
    } else {
        m_pTSX->EqToHz(dRa, dDec, dAzPos, dAltPos);
    }

    azDegToSteps(dAzPos, nAzPos);
    altDegToSteps(dAltPos, nAltPos);
    m_nPrevAzSteps = nAzPos;
    m_nPrevAltSteps = nAltPos;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_mountType : %d\n", timestamp, m_mountType);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Ra            : %3.2f\n", timestamp, dRa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Ha            : %3.2f\n", timestamp, dHa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Dec           : %3.2f\n", timestamp, dDec);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Az            : %3.2f\n", timestamp, dAzPos);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Az steps      : %d\n", timestamp, nAzPos);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Alt           : %3.2f\n", timestamp, dAltPos);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Alt steps     : %d\n", timestamp, nAltPos);
    fflush(Logfile);
#endif
    // if there is an error we might need to retry.
    nErr = setPosition(nAzPos, nAltPos);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] setting position Error : %d\n", timestamp, nErr);
#endif
        return nErr;
    }

    m_bIsSynced = true;
    m_dCurrentRa = dRa;
    m_dCurrentDec = dDec;

    // enable tracking
    nErr = setTrackingRatesSteps(0, 0, 3); // stop first
    nErr |= setTrackingRatesSteps(0xFFFF, 0, 2); // set Ra speed to sidereal
    m_bIsTracking = true;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_bIsSynced   : %s\n", timestamp, m_bIsSynced?"Yes":"No");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_bIsTracking : %s\n", timestamp, m_bIsTracking?"Yes":"No");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_dCurrentRa  : %3.2f\n", timestamp, m_dCurrentRa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_dCurrentDec : %3.2f\n", timestamp, m_dCurrentDec);
    fflush(Logfile);
#endif

    return nErr;
}


int SkyPortalWiFi::isAligned(bool &bAligned)
{
    int nErr = PLUGIN_OK;

    bAligned = m_bIsSynced;
    return nErr;
}

#pragma mark - tracking rates
int SkyPortalWiFi::setTrackingRates(bool bTrackingOn, bool bIgnoreRates, double dTrackRaArcSecPerMin, double dTrackDecArcSecPerMin)
{
    int nErr = PLUGIN_OK;
    double dRaRate, dDecRate;
    int nRaSteps, nDecSteps;

    if(m_bSlewing)
        return nErr;
    

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates]\n", timestamp);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] bTrackingOn = %s\n", timestamp, bTrackingOn?"Yes":"No");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] bIgnoreRates = %s\n", timestamp, bIgnoreRates?"Yes":"No");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] dTrackRaArcSecPerMin = %f\n", timestamp, dTrackRaArcSecPerMin);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] dTrackDecArcSecPerMin = %f\n", timestamp, dTrackDecArcSecPerMin);
    fflush(Logfile);
#endif

    m_bIsTracking = true; // if we set a tracking rate.. we're tracking by default, except for the stop bellow
    m_bSiderealOn = false;

    if(!bTrackingOn) { // stop tracking
        m_bIsTracking = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] setting to Drift\n", timestamp);
        fflush(Logfile);
#endif
        m_dTrackRaArcSecPerMin = 0;
        m_dTrackDecArcSecPerMin = 0;
        nErr = setTrackingRatesSteps(0, 0, 3);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] Error setting to Drift : %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            return nErr;
        }
    }
    else if(bTrackingOn && bIgnoreRates) { // sidereal for Equatorial mount
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] setting to Sidereal\n", timestamp);
        fflush(Logfile);
#endif
        nErr = setTrackingRatesSteps(0, 0, 3); // stop first
        nErr |= setTrackingRatesSteps(0xFFFF, 0, 2); // set Ra speed to sidereal
        m_dTrackRaArcSecPerMin = 15.0410681*60;
        m_dTrackDecArcSecPerMin = 0;
        m_bSiderealOn = true;
    }
    else { // custom rates
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] setting to Custom\n", timestamp);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] dTrackRaArcSecPerMin = %f\n", timestamp, dTrackRaArcSecPerMin);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] dTrackDecArcSecPerMin = %f\n", timestamp, dTrackDecArcSecPerMin);
        fflush(Logfile);
#endif
        m_bSiderealOn = false;
        m_dTrackRaArcSecPerMin = (15.0410681*60) + dTrackRaArcSecPerMin;
        m_dTrackDecArcSecPerMin = dTrackDecArcSecPerMin;

        nRaSteps = (m_dTrackRaArcSecPerMin/3600) * STEPS_PER_DEGREE;
        nDecSteps = (m_dTrackDecArcSecPerMin/3600) * STEPS_PER_DEGREE;
        // convert tacking rate from to steps per min  *  1000*arcmin/minute
        nRaSteps = nRaSteps * TRACK_SCALE;
        nDecSteps = nDecSteps * TRACK_SCALE;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] nRaSteps : %d\n", timestamp, nRaSteps);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] nDecSteps : %d\n", timestamp, nDecSteps);
        fflush(Logfile);
#endif
        nErr = setTrackingRatesSteps(nRaSteps, nDecSteps, 3);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] Error setting to custom : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
    }
    return nErr;
}

int SkyPortalWiFi::getTrackRates(bool &bTrackingOn, double &dTrackRaArcSecPerMin, double &dTrackDecArcSecPerMin)
{
    int nErr = PLUGIN_OK;

    bTrackingOn = m_bIsTracking;
    if(m_bIsTracking) {
        dTrackRaArcSecPerMin = m_dTrackRaArcSecPerMin - (15.0410681*60);
        dTrackDecArcSecPerMin = m_dTrackDecArcSecPerMin;
    }
    else {
        dTrackRaArcSecPerMin = 15.0410681*60; // TSX convention
        dTrackDecArcSecPerMin = 0;
    }

    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getTrackRates]\n", timestamp);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getTrackRates] bTrackingOn = %s\n", timestamp, bTrackingOn?"Yes":"No");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getTrackRates] dTrackRaArcSecPerMin = %f\n", timestamp, dTrackRaArcSecPerMin);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getTrackRates] dTrackDecArcSecPerMin = %f\n", timestamp, dTrackDecArcSecPerMin);
    fflush(Logfile);
#endif

    return nErr;
}


#pragma mark - Limits
int SkyPortalWiFi::getLimits(double &dHoursEast, double &dHoursWest)
{
    int nErr = PLUGIN_OK;
    double dEast, dWest;

    if(m_bLimitCached) {
        dHoursEast = m_dHoursEast;
        dHoursWest = m_dHoursWest;
        return nErr;
    }

    // nErr = getSoftLimitEastAngle(dEast);
    if(nErr)
        return nErr;

    // nErr = getSoftLimitWestAngle(dWest);
    if(nErr)
        return nErr;

    dHoursEast = m_pTSX->hourAngle(dEast);
    dHoursWest = m_pTSX->hourAngle(dWest);

    m_bLimitCached = true;
    m_dHoursEast = dHoursEast;
    m_dHoursWest = dHoursWest;
    return nErr;
}


#pragma mark - Slew

int SkyPortalWiFi::startSlewTo(double dRa, double dDec, bool bFast)
{
    int nErr = PLUGIN_OK;
    bool bAligned;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;

    double dAlt, dAz;
    int nAltPos, nAzPos;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to Ra = %3.2f, Dec = %3.2f\n", timestamp, dRa, dDec);
    fflush(Logfile);
#endif

    m_dGotoRATarget = dRa;
    m_dGotoDECTarget = dDec;
    
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 6;
    Cmd[SRC_DEV] = PC;
    if(bFast)
        Cmd[CMD_ID] = MC_GOTO_FAST;
    else
        Cmd[CMD_ID] = MC_GOTO_SLOW;

    nErr = isAligned(bAligned);
    if(nErr)
        return nErr;


    if(m_bIsMountEquatorial) {
        // do Az (Ra)
        
        Cmd[DST_DEV] = AZM;
        azDegToSteps(dRa, nAzPos);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to nAzPos = %d\n", timestamp, nAzPos);
        fflush(Logfile);
#endif
        Cmd[5] = (unsigned char)((nAzPos & 0x00ff0000) >> 16);
        Cmd[6] = (unsigned char)((nAzPos & 0x0000ff00) >> 8);
        Cmd[7] = (unsigned char)( nAzPos & 0x000000ff);
        Cmd[8] = checksum(Cmd);
        nErr = SendCommand(Cmd, Resp, true);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] ERROR Slewing to nAzPos = %d\n", timestamp, nAzPos);
            fflush(Logfile);
#endif
            return nErr;
        }
        // do Alt (Dec)
        
        Cmd[DST_DEV] = ALT;
        altDegToSteps(dDec, nAltPos);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to nAltPos = %d\n", timestamp, nAltPos);
        fflush(Logfile);
#endif
        Cmd[5] = (unsigned char)((nAltPos & 0x00ff0000) >> 16);
        Cmd[6] = (unsigned char)((nAltPos & 0x0000ff00) >> 8);
        Cmd[7] = (unsigned char)( nAltPos & 0x000000ff);
        Cmd[8] = checksum(Cmd);
        nErr = SendCommand(Cmd, Resp, true);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] ERROR Slewing to nAltPos = %d\n", timestamp, nAltPos);
            fflush(Logfile);
#endif
            return nErr;
        }
    }
    else {                //// Az/Alt mode //////
        m_pTSX->EqToHz(dRa, dDec, dAz, dAlt);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to dAz = %3.2f\n", timestamp, dAz);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to dAlt = %3.2f\n", timestamp, dAlt);
        fflush(Logfile);
#endif
        // do Az
        
        Cmd[DST_DEV] = AZM;
        azDegToSteps(dAz, nAzPos);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to nAzPos = %d\n", timestamp, nAzPos);
        fflush(Logfile);
#endif
        Cmd[5] = (unsigned char)((nAzPos & 0x00ff0000) >> 16);
        Cmd[6] = (unsigned char)((nAzPos & 0x0000ff00) >> 8);
        Cmd[7] = (unsigned char)( nAzPos & 0x000000ff);
        Cmd[8] = checksum(Cmd);
        nErr = SendCommand(Cmd, Resp, true);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] ERROR Slewing to nAzPos = %d\n", timestamp, nAzPos);
            fflush(Logfile);
#endif
            return nErr;
        }
        // do Alt
        
        Cmd[DST_DEV] = ALT;
        altDegToSteps(dAlt, nAltPos);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to nAltPos = %d\n", timestamp, nAltPos);
        fflush(Logfile);
#endif
        Cmd[5] = (unsigned char)((nAltPos & 0x00ff0000) >> 16);
        Cmd[6] = (unsigned char)((nAltPos & 0x0000ff00) >> 8);
        Cmd[7] = (unsigned char)( nAltPos & 0x000000ff);
        Cmd[8] = checksum(Cmd);
        nErr = SendCommand(Cmd, Resp, true);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] ERROR Slewing to nAltPos = %d\n", timestamp, nAltPos);
            fflush(Logfile);
#endif
            return nErr;
        }
    }
    m_bIsTracking = false;
    if(bFast)
        m_bGotoFast = true;
    else
        m_bGotoFast = false;

    m_bSlewing = true;
    
    return nErr;
}


int SkyPortalWiFi::startOpenSlew(const MountDriverInterface::MoveDir Dir, unsigned int nRate)
{
    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;
    int nSlewRate;

    m_nOpenLoopDir = Dir;
    m_bSlewing = true;
    
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::startOpenSlew] setting to Dir %d\n", timestamp, Dir);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::startOpenSlew] Setting rate to %d\n", timestamp, nRate);
    fflush(Logfile);
#endif

    // select rate
    nSlewRate = atoi(m_aszSlewRateNames[nRate]); // 0 to 9

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::startOpenSlew] nSlewRate =  %d\n", timestamp, nSlewRate);
    fflush(Logfile);
#endif

    // figure out direction
    switch(Dir){
        case MountDriverInterface::MD_NORTH:
            Cmd[DST_DEV] = ALT;
            Cmd[CMD_ID] = MC_MOVE_POS;
            break;
        case MountDriverInterface::MD_SOUTH:
            Cmd[DST_DEV] = ALT;
            Cmd[CMD_ID] = MC_MOVE_NEG;
            break;
        case MountDriverInterface::MD_EAST:
            Cmd[DST_DEV] = AZM;
            Cmd[CMD_ID] = MC_MOVE_POS;
            break;
        case MountDriverInterface::MD_WEST:
            Cmd[DST_DEV] = AZM;
            Cmd[CMD_ID] = MC_MOVE_NEG;
            break;
    }
    Cmd[5] = nSlewRate & 0xFF;
    Cmd[6] = checksum(Cmd);
    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startOpenSlew] ERROR Slewing in dir %d on axis  %d : %d\n", timestamp, Cmd[CMD_ID], Cmd[DST_DEV], nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    return nErr;
}

int SkyPortalWiFi::stopOpenLoopMove()
{
    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::stopOpenLoopMove] Dir was %d\n", timestamp, m_nOpenLoopDir);
    fflush(Logfile);
#endif

    
    switch(m_nOpenLoopDir){
        case MountDriverInterface::MD_NORTH:
            Cmd[DST_DEV] = ALT;
            Cmd[CMD_ID] = MC_MOVE_POS;
            break;
        case MountDriverInterface::MD_SOUTH:
            Cmd[DST_DEV] = ALT;
            Cmd[CMD_ID] = MC_MOVE_NEG;
            break;
        case MountDriverInterface::MD_EAST:
            Cmd[DST_DEV] = AZM;
            Cmd[CMD_ID] = MC_MOVE_POS;
            break;
        case MountDriverInterface::MD_WEST:
            Cmd[DST_DEV] = AZM;
            Cmd[CMD_ID] = MC_MOVE_NEG;
            break;
    }
    Cmd[5] = 0; // stop
    Cmd[6] = checksum(Cmd);
    nErr = SendCommand(Cmd, Resp, true);
    if(nErr)
        return nErr;

    // set speed back to what it was
    setTrackingRates(m_bIsTracking, true, 0, 0);
    m_bSlewing = false;

    return nErr;
}

int SkyPortalWiFi::isSlewToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;
    unsigned char szHexMessage[LOG_BUFFER_SIZE];
    bool bAzComplete, baltComplete;

    if(!m_bSlewing) {
        bComplete = true;
        return nErr;
    }
    bComplete = false;
    bAzComplete = false;
    baltComplete = false;

    if(m_Timer.GetElapsedSeconds()<2) {
        // we can't talk to the controller too often when slewing
        return nErr;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
	hexdump(Cmd.data(), szHexMessage, (int)(Cmd[1])+3, LOG_BUFFER_SIZE);

    fprintf(Logfile, "[%s] [SkyPortalWiFi::isSlewToComplete]\n", timestamp);
    fflush(Logfile);
#endif

    
    // check slew on AZM
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = AZM;
    Cmd[CMD_ID] = MC_SLEW_DONE;
    Cmd[5] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    if(nErr)
        return nErr;

    if(Resp[0] == 0xFF)
        baltComplete = true;

    
    // check slew on alt
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = ALT;
    Cmd[CMD_ID] = MC_SLEW_DONE;
    Cmd[5] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    if(nErr)
        return nErr;

    if(Resp[0] == 0xFF)
        bAzComplete = true;

    bComplete = bAzComplete & baltComplete;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
	hexdump(Cmd.data(), szHexMessage, (int)(Cmd[1])+3, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] SkyPortalWiFi::isSlewToComplete baltComplete = %s\n", timestamp, baltComplete?"Yes":"No");
    fprintf(Logfile, "[%s] SkyPortalWiFi::isSlewToComplete bAzComplete  = %s\n", timestamp, bAzComplete?"Yes":"No");
    fprintf(Logfile, "[%s] SkyPortalWiFi::isSlewToComplete bComplete    = %s\n", timestamp, bComplete?"Yes":"No");
    fflush(Logfile);
#endif

    m_Timer.Reset();

    if(bComplete && m_bGotoFast) {
        bComplete = false;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        fprintf(Logfile, "[%s] SkyPortalWiFi::isSlewToComplete doing slow goto\n", timestamp);
        fflush(Logfile);
#endif
        nErr = startSlewTo(m_dGotoRATarget, m_dGotoDECTarget, false ); // now do a slow goto
        return nErr;
    }
    else if(bComplete & !m_bIsTracking) {
        m_bSlewing = false;
        m_dCurrentRa = m_dGotoRATarget;
        m_dCurrentDec = m_dGotoDECTarget;
        azDegToSteps(m_dCurrentRa, m_nPrevAzSteps);
        altDegToSteps(m_dCurrentDec, m_nPrevAltSteps);

        if(m_bIsParking) {
            m_bIsTracking = false;
            setTrackingRatesSteps(0, 0, 3); // stop
        }
        else {
            m_bIsTracking = true;
            if(m_bIsMountEquatorial)
                nErr = setTrackingRates(true, true, 0, 0); // set to sidereal
            // else not needed, it'll be set in the getRaDec for Alt/Az mount as we need to compute the tracking rate.
        }
    }

    return nErr;
}

int SkyPortalWiFi::gotoPark(double dRa, double dDec)
{
    int nErr = PLUGIN_OK;

    // goto Park
    nErr = startSlewTo(dRa, dDec);
    m_dParkRa = dRa;
    m_dParkDec = dDec;

    m_bIsParking = true;
    m_bIsTracking = false;
    m_bSlewing = true;
    return nErr;
}


int SkyPortalWiFi::getAtPark(bool &bParked)
{
    int nErr = PLUGIN_OK;
    int nAltSteps, nAzSteps;
    double dAz, dAlt;
    double dRa, dDec;

    bParked = false;
    if(m_bIsParking) {
        nErr = isSlewToComplete(bParked);
        if(bParked)
            m_bIsParking = false; // done parking
        return nErr;
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getAtPark]\n", timestamp);
    fflush(Logfile);
#endif

    // there is no real pack position status from the mount, so do our best
    //  if this is called outside of a parking call.
    getPosition(nAzSteps, nAltSteps);
    altStepsToDeg(nAltSteps, dAlt);
    azStepsToDeg(nAzSteps, dAz);

    m_pTSX->HzToEq(dAz, dAlt, dRa, dDec);

    if( floor(dRa) == floor(m_dParkRa) && floor(dDec) == floor(m_dParkDec))
        bParked = true;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getAtPark] bParked = %s\n", timestamp, bParked?"Yes":"No");
    fflush(Logfile);
#endif

    return nErr;
}

int SkyPortalWiFi::unPark()
{
    int nErr = PLUGIN_OK;
    bool bAligned;

    // are we aligned ?
    isAligned(bAligned);
    // if not
    if(!bAligned) {
        // sync to Park position ?
        // but we don't know the park position.
    }
    // start tracking
    nErr = setTrackingRates(true, true, 0, 0);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::unPark] Error setting track rate to Sidereal\n", timestamp);
        fflush(Logfile);
#endif
    }
    return nErr;
}

void SkyPortalWiFi::setParkPosition(double dRa, double dDec)
{
    m_dParkRa = dRa;
    m_dParkDec = dDec;
}

int SkyPortalWiFi::Abort()
{
    int nErr = PLUGIN_OK;

    nErr = setTrackingRatesSteps(0, 0, 3);
    nErr |= moveAlt(0);
    nErr |= moveAz(0);

    m_bIsTracking = false;
    m_bSiderealOn = false;
    m_bGotoFast = false;
    m_bSlewing = false;

    return nErr;
}

int SkyPortalWiFi::moveAz(int nSteps)
{
    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;
    unsigned char cCmd = MC_MOVE_POS;

    if(nSteps<0) {
        nSteps = abs(nSteps);
        cCmd = MC_MOVE_NEG;
    }

    
    // set AZM
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 6;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = AZM;
    Cmd[CMD_ID] = cCmd;
    Cmd[5] = (unsigned char)((nSteps & 0x00ff0000) >> 16);
    Cmd[6] = (unsigned char)((nSteps & 0x0000ff00) >> 8);
    Cmd[7] = (unsigned char)( nSteps & 0x000000ff);
    Cmd[8] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    return nErr;
}

int SkyPortalWiFi::moveAlt(int nSteps)
{
    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;
    unsigned char cCmd = MC_MOVE_POS;

    if(nSteps<0) {
        nSteps = abs(nSteps);
        cCmd = MC_MOVE_NEG;
    }
    
    // set ALT
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 6;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = ALT;
    Cmd[CMD_ID] = cCmd;
    Cmd[5] = (unsigned char)((nSteps & 0x00ff0000) >> 16);
    Cmd[6] = (unsigned char)((nSteps & 0x0000ff00) >> 8);
    Cmd[7] = (unsigned char)( nSteps & 0x000000ff);
    Cmd[8] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);

    return nErr;
}

#pragma mark - Special commands & functions

int SkyPortalWiFi::setTrackingRatesSteps( int nAzRate, int nAltRate, int nDataLen)
{
    int nErr = PLUGIN_OK;
    Buffer_t Cmd(SERIAL_BUFFER_SIZE);
    Buffer_t Resp;
    unsigned char cCmd;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nAzRate = %d\n", timestamp, nAzRate);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nAltRate = %d\n", timestamp, nAltRate);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nDataLen = %d\n", timestamp, nDataLen);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] latitude = %3.8f\n", timestamp, m_pTSX->latitude());
    fflush(Logfile);
#endif

    

    cCmd = MC_SET_POS_GUIDERATE;

    // special case for sidereal south
    if(nAzRate == 0xFFFF &&  m_pTSX->latitude()<0)
        cCmd = MC_SET_NEG_GUIDERATE;
    
    if(nAzRate <0 && m_pTSX->latitude() >= 0) {
        nAzRate = abs(nAzRate);
        cCmd = MC_SET_NEG_GUIDERATE;
    }

    if(nAzRate > 0 && m_pTSX->latitude() < 0) {
        cCmd = MC_SET_NEG_GUIDERATE;
    }


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nAzRate = %06X\n", timestamp, nAzRate);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] Az dir = %s\n", timestamp, cCmd == MC_SET_NEG_GUIDERATE?"Neg":"Pos");
    fflush(Logfile);
#endif

    
    // set AZM
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3 + nDataLen;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = AZM;
    Cmd[CMD_ID] = cCmd;
    if(nDataLen==2) {
        Cmd[5] = (unsigned char)((nAzRate & 0x0000ff00) >> 8);
        Cmd[6] = (unsigned char)(nAzRate & 0x000000ff);
        Cmd[7] = checksum(Cmd);
    }
    else if (nDataLen == 3) {
        Cmd[5] = (unsigned char)((nAzRate & 0x00ff0000) >> 16);
        Cmd[6] = (unsigned char)((nAzRate & 0x0000ff00) >> 8);
        Cmd[7] = (unsigned char)(nAzRate & 0x000000ff);
        Cmd[8] = checksum(Cmd);
    }
    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] ERROR setting Alt tracking rate = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    cCmd = MC_SET_POS_GUIDERATE;
    if(nAltRate <0) {
        nAltRate = abs(nAltRate);
        cCmd = MC_SET_NEG_GUIDERATE;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nAltRate = %06X\n", timestamp, nAltRate);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] Alt dir = %s\n", timestamp, cCmd == MC_SET_NEG_GUIDERATE?"Neg":"Pos");
    fflush(Logfile);
#endif

    
    // set ALT
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3+nDataLen;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = ALT;
    Cmd[CMD_ID] = cCmd;
    if(nDataLen==2) {
        Cmd[5] = (unsigned char)((nAltRate & 0x0000ff00) >> 8);
        Cmd[6] = (unsigned char)(nAltRate & 0x000000ff);
        Cmd[7] = checksum(Cmd);
    }
    else if (nDataLen == 3) {
        Cmd[5] = (unsigned char)((nAltRate & 0x00ff0000) >> 16);
        Cmd[6] = (unsigned char)((nAltRate & 0x0000ff00) >> 8);
        Cmd[7] = (unsigned char)( nAltRate & 0x000000ff);
        Cmd[8] = checksum(Cmd);
    }
    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] ERROR setting Alt tracking rate = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    return nErr;
}

void SkyPortalWiFi::convertDecDegToDDMMSS(double dDeg, char *szResult, char &cSign, unsigned int size)
{
    int DD, MM, SS;
    double mm, ss;

    // convert dDeg decimal value to sDD:MM:SS

    cSign = dDeg>=0?'+':'-';
    dDeg = fabs(dDeg);
    DD = int(dDeg);
    mm = dDeg - DD;
    MM = int(mm*60);
    ss = (mm*60) - MM;
    SS = int(ceil(ss*60));
    snprintf(szResult, size, "%02d:%02d:%02d", DD, MM, SS);
}

int SkyPortalWiFi::convertDDMMSSToDecDeg(const char *szStrDeg, double &dDecDeg)
{
    int nErr = PLUGIN_OK;
    std::vector<std::string> vFieldsData;

    dDecDeg = 0;

    nErr = parseFields(szStrDeg, vFieldsData, ':');
    if(nErr)
        return nErr;

    if(vFieldsData.size() >= 3) {
        dDecDeg = atof(vFieldsData[0].c_str()) + atof(vFieldsData[1].c_str())/60 + atof(vFieldsData[1].c_str())/3600;
    }
    else
        nErr = ERR_BADFORMAT;

    return nErr;
}

void SkyPortalWiFi::convertRaToHHMMSSt(double dRa, char *szResult, unsigned int size)
{
    int HH, MM;
    double hh, mm, SSt;

    // convert Ra value to HH:MM:SS.T before passing them to the SkyPortalWiFi
    HH = int(dRa);
    hh = dRa - HH;
    MM = int(hh*60);
    mm = (hh*60) - MM;
    SSt = mm * 60;
    snprintf(szResult,SERIAL_BUFFER_SIZE, "%02d:%02d:%02.1f", HH, MM, SSt);
}

int SkyPortalWiFi::convertHHMMSStToRa(const char *szStrRa, double &dRa)
{
    int nErr = PLUGIN_OK;
    std::vector<std::string> vFieldsData;

    dRa = 0;

    nErr = parseFields(szStrRa, vFieldsData, ':');
    if(nErr)
        return nErr;

    if(vFieldsData.size() >= 3) {
        dRa = atof(vFieldsData[0].c_str()) + atof(vFieldsData[1].c_str())/60 + atof(vFieldsData[1].c_str())/3600;
    }
    else
        nErr = ERR_BADFORMAT;

    return nErr;
}

void SkyPortalWiFi::azStepsToDeg(int nSteps, double &dDeg)
{

    dDeg = double(nSteps) / STEPS_PER_DEGREE;
}

void SkyPortalWiFi::azDegToSteps(double dDeg, int &nSteps)
{
    nSteps = int(dDeg * STEPS_PER_DEGREE);
}

double  SkyPortalWiFi::stepToHa(int nSteps)
{
    return double(nSteps) / double(STEPS_PER_REVOLUTION) * 24;
}

int SkyPortalWiFi::haToSteps(double dHa)
{
     return int((double(STEPS_PER_REVOLUTION) * dHa)/ 24.0);
}


void SkyPortalWiFi::altStepsToDeg(int nSteps, double &dDeg)
{

	nSteps = nSteps - (STEPS_PER_REVOLUTION / 2); // STEPS_PER_REVOLUTION / 2 is the 0 Deg. reference
    dDeg = double(nSteps) / STEPS_PER_DEGREE;
}

void SkyPortalWiFi::altDegToSteps(double dDeg, int &nSteps)
{
    nSteps = int(dDeg * STEPS_PER_DEGREE);
	nSteps = nSteps + (STEPS_PER_REVOLUTION / 2); // STEPS_PER_REVOLUTION / 2 is the 0 Deg. reference
}

int SkyPortalWiFi::parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = PLUGIN_OK;
    std::string sSegment;
    std::stringstream ssTmp(pszIn);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
        svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        nErr = ERR_BADFORMAT;
    }
    return nErr;
}


