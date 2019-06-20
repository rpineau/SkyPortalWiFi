#include "SkyPortalWiFi.h"

// Constructor for SkyPortalWiFi
SkyPortalWiFi::SkyPortalWiFi()
{

	m_bIsConnected = false;
    m_bIsSynced = false;
    m_bDebugLog = true;

    m_bMountIsGEM = false; // alt / Az is the default

    m_bJNOW = false;

    m_b24h = false;
    m_bDdMmYy = false;
    m_bTimeSetOnce = false;
    m_bLimitCached = false;

    m_bIsTracking = false;
    m_bSiderealOn = false;
    m_bGotoFast = false;
    m_nPrevAltSteps = 0;
    m_nPrevAzSteps = 0;

#ifdef SKYPORTAL_DEBUG
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

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [SkyPortalWiFi] New Constructor Called\n", timestamp);
    fflush(Logfile);
#endif

}


SkyPortalWiFi::~SkyPortalWiFi(void)
{
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [SkyPortalWiFi] Destructor Called\n", timestamp );
    fflush(Logfile);
#endif
#ifdef SKYPORTAL_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int SkyPortalWiFi::Connect(char *pszPort)
{
    int nErr = SKYPORTAL_OK;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [SkyPortalWiFi::Disconnect] Called\n", timestamp);
    fflush(Logfile);
#endif
	if (m_bIsConnected) {
        if(m_pSerx){
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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

	return SB_OK;
}


int SkyPortalWiFi::getNbSlewRates()
{
    return SkyPortalWiFi_NB_SLEW_SPEEDS;
}

int SkyPortalWiFi::getRateName(int nZeroBasedIndex, char *pszOut, unsigned int nOutMaxSize)
{
    if (nZeroBasedIndex > SkyPortalWiFi_NB_SLEW_SPEEDS)
        return SKYPORTAL_ERROR;

    strncpy(pszOut, m_aszSlewRateNames[nZeroBasedIndex], nOutMaxSize);

    return SKYPORTAL_OK;
}

#pragma mark - SkyPortalWiFi communication

int SkyPortalWiFi::SendCommand(const Buffer_t Cmd, Buffer_t Resp, const bool bExpectResponse)
{
	int nErr = SKYPORTAL_OK;
	unsigned long  ulBytesWrite;
	unsigned char szHexMessage[LOG_BUFFER_SIZE];
	int timeout = 0;
	int nRespLen;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	m_pSerx->purgeTxRx();
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(Cmd.data(), szHexMessage, (int)(Cmd[1])+3, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] Sending %s\n", timestamp, szHexMessage);
	fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] packet size is %d\n", timestamp, (int)(Cmd[1])+3);
	fflush(Logfile);
#endif
	nErr = m_pSerx->writeFile((void *)Cmd.data(), (unsigned long)(Cmd[1])+3, ulBytesWrite);
	m_pSerx->flushTx();

	if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
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
				return COMMAND_FAILED;
			}
			// read response
			nErr = ReadResponse(Resp, nRespLen);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
			if(Resp.size() && Resp[DST_DEV]==PC) { // filter out command echo
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				hexdump(Resp.data(), szHexMessage, Resp[3]+1, LOG_BUFFER_SIZE);
				fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response \"%s\"\n", timestamp, szHexMessage);
				fflush(Logfile);
			}
#endif
			if(nErr)
				return nErr;
			m_pSleeper->sleep(100);
			timeout++;
		} while(Resp.size() && Resp[DST_DEV]!=PC);

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(Resp.data(), szHexMessage, Resp[3]+1, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CCelestronFocus::SendCommand] response copied to pszResult : \"%s\"\n", timestamp, szHexMessage);
		fflush(Logfile);
#endif
	}
	return nErr;
}


int SkyPortalWiFi::ReadResponse(Buffer_t RespBuffer, int &nLen)
{
	int nErr = SKYPORTAL_OK;
	unsigned long ulBytesRead = 0;
	unsigned char cHexMessage[LOG_BUFFER_SIZE];
	unsigned char cChecksum;
	unsigned char pRespBuffer[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	memset(pRespBuffer, 0, (size_t) SERIAL_BUFFER_SIZE);

	// Look for a SOM starting character, until timeout occurs
	while (*pRespBuffer != SOM && nErr == SKYPORTAL_OK) {
		nErr = m_pSerx->readFile(pRespBuffer, 1, ulBytesRead, MAX_TIMEOUT);
		if (ulBytesRead !=1) // timeout
			nErr = SKYPORTAL_BAD_CMD_RESPONSE;
	}

	if(*pRespBuffer != SOM || nErr != SKYPORTAL_OK)
		return ERR_CMDFAILED;

	// Read message length
	nErr = m_pSerx->readFile(pRespBuffer + 1, 1, ulBytesRead, MAX_TIMEOUT);
	if (nErr != SKYPORTAL_OK || ulBytesRead!=1)
		return ERR_CMDFAILED;

	nLen = int(pRespBuffer[1]);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pRespBuffer, cHexMessage, int(pRespBuffer[1])+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] nLen = %d\n", timestamp, nLen);
	fflush(Logfile);
#endif

	// Read the rest of the message
	nErr = m_pSerx->readFile(pRespBuffer + 2, nLen + 1, ulBytesRead, MAX_TIMEOUT); // the +1 on nLen is to also read the checksum
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	hexdump(pRespBuffer, cHexMessage, int(pRespBuffer[1])+2, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] ulBytesRead = %lu\n", timestamp, ulBytesRead);
	fflush(Logfile);
#endif
	if(nErr || ulBytesRead != (nLen+1)) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		hexdump(pRespBuffer, cHexMessage, int(pRespBuffer[1])+2, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] error\n", timestamp);
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] got %s\n", timestamp, cHexMessage);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

	// verify checksum
	cChecksum = checksum(pRespBuffer);
	if (cChecksum != *(pRespBuffer+nLen+2)) {
		nErr = ERR_CMDFAILED;
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 3
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CCelestronFocus::readResponse] Calculated checksume is %02X, message checksum is %02X\n", timestamp, cChecksum, *(pRespBuffer+nLen+2));
		fflush(Logfile);
#endif
	}
	nLen = int(pRespBuffer[1])+2;
	RespBuffer.assign(pRespBuffer, pRespBuffer+nLen);
	return nErr;
}


unsigned char SkyPortalWiFi::checksum(const unsigned char *cMessage)
{
	int nIdx;
	char cChecksum = 0;

	for (nIdx = 1; nIdx < cMessage[1]+2; nIdx++) {
		cChecksum -= cMessage[nIdx];
	}
	return (unsigned char)cChecksum;
}

uint8_t SkyPortalWiFi::checksum(const Buffer_t cMessage)
{
	int nIdx;
	char cChecksum = 0;

	for (nIdx = 1; nIdx < cMessage[1]+2; nIdx++) {
		cChecksum -= cMessage[nIdx];
	}
	return (uint8_t)cChecksum;
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
    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
    Buffer_t Resp;
    char AMZVersion[SERIAL_BUFFER_SIZE];
    char ALTVersion[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    Cmd.assign (SERIAL_BUFFER_SIZE, 0);

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] Error getting AZM version : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    snprintf(AMZVersion, nStrMaxLen,"%d.%d", Resp[5] , Resp[6]);

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] AZM version : %s\n", timestamp, AMZVersion);
    fflush(Logfile);
#endif

	Cmd.assign (SERIAL_BUFFER_SIZE, 0);


#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getFirmwareVersion] Error getting AZM version : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    snprintf(ALTVersion, nStrMaxLen,"%d.%d", Resp[5] , Resp[6]);

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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

int SkyPortalWiFi::getRaAndDec(double &dRa, double &dDec)
{
    int nErr = SKYPORTAL_OK;

    double dAlt = 0.0, dAz = 0.0;
    double dNewAlt, dNewAz;         // position where we should be in Deg.
    int nAltSteps,nAzSteps;         // current mount position in steps
    int nNewAltSteps,nNewAzSteps;   // position where we should be in steps
    int nAltError, nAzError;        // position error
    int nAltRate, nAzRate;          // guide rate
    float fSecondsDelta;
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec]\n", timestamp);
    fflush(Logfile);
#endif

    nErr = getPosition(nAzSteps, nAltSteps);
    if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Error getting position : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(m_bMountIsGEM) {
        stepsToDeg(getFixedAz(nAzSteps), dRa);
        stepsToDeg(getFixedAlt(nAltSteps), dDec);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Ra : %3.2f\n", timestamp, dRa);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Dec : %3.2f\n", timestamp, dDec);
        fflush(Logfile);
#endif
        return nErr;
    }

    // Az Mount.
    stepsToDeg(getFixedAz(nAzSteps), dAz);
    stepsToDeg(getFixedAlt(nAltSteps), dAlt);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] dAz : %3.2f\n", timestamp, dAz);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] dAlt : %3.2f\n", timestamp, dAlt);
    fflush(Logfile);
#endif

    m_pTsx->HzToEq(dAz, dAlt, dRa, dDec); // convert  Az,Alt to TSX Ra,Dec

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Ra : %3.2f\n", timestamp, dRa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Dec : %3.2f\n", timestamp, dDec);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] m_dCurrentRa : %3.2f\n", timestamp, m_dCurrentRa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] m_dCurrentDec : %3.2f\n", timestamp, m_dCurrentDec);
    fflush(Logfile);
#endif

    
    // now deal with Az/Alt mount as we need to adjust the tracking rate all the time
    if(m_bIsTracking && m_bIsSynced && !m_bMountIsGEM) {
        // adjust tracking rate every 30 seconds at most
        fSecondsDelta = m_trakingTimer.GetElapsedSeconds();
        if( fSecondsDelta<= 30) {
            return nErr;
        }
        m_trakingTimer.Reset();

        // It should be pointing to the latest goto/move
        // get Alt/Az for where it should be pointing
        m_pTsx->EqToHz(m_dCurrentRa, m_dCurrentDec, dNewAz, dNewAlt);
        // conver to steps
        degToSteps(dNewAz, nNewAzSteps);
        degToSteps(dNewAlt, nNewAltSteps);

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

        // Track function needs rates in 1000*arcmin/minute
        nAzRate  = TRACK_SCALE * nAzRate;
        nAltRate = TRACK_SCALE * nAltRate;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] fSecondsDelta   : %3.2f\n", timestamp, fSecondsDelta);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Az        : %3.2f\n", timestamp, dAz);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Az steps  : %d\n", timestamp, nAzSteps);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Alt       : %3.2f\n", timestamp, dAlt);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Scope Alt steps : %d\n", timestamp, nAltSteps);

        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Target Az       : %3.2f\n", timestamp, dNewAz);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Targe Az steps  : %d\n", timestamp, nNewAzSteps);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Target Alt      : %3.2f\n", timestamp, dNewAlt);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Taret Alt steps : %d\n", timestamp, nNewAltSteps);

        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Az error        : %d\n", timestamp, nAzError);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Alt error       : %d\n", timestamp, nAltError);

        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Az rate         : %d\n", timestamp, nAzRate);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getRaAndDec] Alt rate        : %d\n", timestamp, nAltRate);
        fflush(Logfile);
#endif

        // update the tracking rate
        nErr = setTrackingRatesSteps(nAzRate, nAltRate, 3);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
    int nErr = SKYPORTAL_OK;
	Buffer_t Cmd;
	Buffer_t Resp;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSkyPortalWiFi::getPosition]\n", timestamp);
    fflush(Logfile);
#endif


#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getPosition] error getting nAzSteps : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    // convert counters to proper degrees
    nAzSteps = Resp[5]<<16 | Resp[6]<<8 | Resp[7];

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSkyPortalWiFi::getPosition] Get Alt\n", timestamp);
    fflush(Logfile);
#endif
	Cmd.assign (SERIAL_BUFFER_SIZE, 0);
    // get ALT
    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;
    Cmd[DST_DEV] = ALT;
    Cmd[CMD_ID] = MC_GET_POSITION;
    Cmd[5] = checksum(Cmd);

    nErr = SendCommand(Cmd, Resp, true);
    if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::getPosition] error getting nAltSteps : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
    // convert counters to proper degrees
    nAltSteps = Resp[5]<<16 | Resp[6]<<8 | Resp[7];

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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

    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
    Buffer_t Resp;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition]\n", timestamp);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] nAzSteps : %d\n", timestamp, nAzSteps);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] nAltSteps : %d\n", timestamp, nAltSteps);
    fflush(Logfile);
#endif


#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setPosition] error setting nAzSteps : %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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


MountTypeInterface::Type SkyPortalWiFi::mountType()
{
    if(m_bMountIsGEM)
        return MountTypeInterface::Asymmetrical_Equatorial;
    else
        return MountTypeInterface::AltAz;
}


#pragma mark - Sync and Cal
int SkyPortalWiFi::syncTo(double dRa, double dDec)
{
    int nErr = SKYPORTAL_OK;
    double dAltPos, dAzPos;
    int nAltPos, nAzPos;

    m_bIsSynced = false;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo]\n", timestamp);
    fflush(Logfile);
#endif

    if(m_bMountIsGEM) {
        dAzPos = dRa;
        dAltPos = dDec;
    } else {
        m_pTsx->EqToHz(dRa, dDec, dAzPos, dAltPos);
    }

    degToSteps(dAzPos, nAzPos);
    degToSteps(dAltPos, nAltPos);
    m_nPrevAzSteps = nAzPos;
    m_nPrevAltSteps = nAltPos;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_bMountIsGEM : %s\n", timestamp, m_bMountIsGEM?"Yes":"No");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] Ra            : %3.2f\n", timestamp, dRa);
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_bIsSynced   : %s\n", timestamp, m_bIsSynced?"Yes":"No");
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_dCurrentRa  : %3.2f\n", timestamp, m_dCurrentRa);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::syncTo] m_dCurrentDec : %3.2f\n", timestamp, m_dCurrentDec);
    fflush(Logfile);
#endif

    return nErr;
}


int SkyPortalWiFi::isAligned(bool &bAligned)
{
    int nErr = SKYPORTAL_OK;

    bAligned = m_bIsSynced;
    return nErr;
}

#pragma mark - tracking rates
int SkyPortalWiFi::setTrackingRates(bool bTrackingOn, bool bIgnoreRates, double dTrackRaArcSecPerMin, double dTrackDecArcSecPerMin)
{
    int nErr = SKYPORTAL_OK;
    double dRaRate, dDecRate;
    int nAltRate, nAzRate;
    int nRaSteps, nDecSteps;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] setting to Drift\n", timestamp);
        fflush(Logfile);
#endif
        nErr = setTrackingRatesSteps(0, 0, 3);
        if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] Error setting to Drift : %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            return nErr;
        }
    }
    else if(bTrackingOn && bIgnoreRates && m_bMountIsGEM) { // sidereal for Equatorial mount
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] setting to Sidereal\n", timestamp);
        fflush(Logfile);
#endif
        nErr = setTrackingRatesSteps(0, 0, 3); // stop first
        nErr |= setTrackingRatesSteps(0xFFFF, 0, 2); // set Ra to sidereal
        if(nErr) {
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] Error setting to Sidereal : %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            return nErr;
        }
        m_bSiderealOn = true;
    }
    else if(bTrackingOn && bIgnoreRates && !m_bMountIsGEM) { // need to compute tracking rate for Alt/Az
        // convert tacking rate in steps per minute
        dRaRate = dTrackRaArcSecPerMin / 3600; //deg per min
        dDecRate = dTrackDecArcSecPerMin / 3600; //deg per min

    }
    else { // custom rates
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] setting to Custom\n", timestamp);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] dTrackRaArcSecPerMin = %f\n", timestamp, dTrackRaArcSecPerMin);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] dTrackDecArcSecPerMin = %f\n", timestamp, dTrackDecArcSecPerMin);
        fflush(Logfile);
#endif
        if(bIgnoreRates)
            m_bSiderealOn = true;
        dRaRate = dTrackRaArcSecPerMin / 3600; //deg per min
        dDecRate = dTrackDecArcSecPerMin / 3600; //deg per min
        if(m_bMountIsGEM) {
            degToSteps(dRaRate, nRaSteps); // steps per min
            degToSteps(dDecRate, nDecSteps); // steps per min
            // convert tacking rate from to steps per min  *  1000*arcmin/minute
            nRaSteps = nRaSteps * TRACK_SCALE;
            nDecSteps = nDecSteps * TRACK_SCALE;
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] nRaSteps : %d\n", timestamp, nRaSteps);
            fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] nDecSteps : %d\n", timestamp, nDecSteps);
            fflush(Logfile);
#endif
            nErr = setTrackingRatesSteps(nRaSteps, nDecSteps, 3);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRates] Error setting to custom : %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
        }
        else {
            // need to compute tracking rate for Alt/As
            // for now .. no supported
            return ERR_COMMANDNOTSUPPORTED;

            // nErr = setTrackingRatesSteps(nAzRate, nAltRate, 3);
        }
    }
    return nErr;
}

int SkyPortalWiFi::getTrackRates(bool &bTrackingOn, double &dTrackRaArcSecPerMin, double &dTrackDecArcSecPerMin)
{
    int nErr = SKYPORTAL_OK;


    if(m_bMountIsGEM && m_bSiderealOn && m_bIsTracking) {
        bTrackingOn = true;
        dTrackRaArcSecPerMin = 15.0410681 * 60;
        dTrackDecArcSecPerMin = 0.0;
        return nErr;
    }
    else {
        bTrackingOn = false;
        dTrackRaArcSecPerMin = -1000.0 * 60;
        dTrackDecArcSecPerMin = -1000.0 * 60;
    }
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
    int nErr = SKYPORTAL_OK;
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

    dHoursEast = m_pTsx->hourAngle(dEast);
    dHoursWest = m_pTsx->hourAngle(dWest);

    m_bLimitCached = true;
    m_dHoursEast = dHoursEast;
    m_dHoursWest = dHoursWest;
    return nErr;
}


#pragma mark - Slew

int SkyPortalWiFi::startSlewTo(double dRa, double dDec, bool bFast)
{
    int nErr = SKYPORTAL_OK;
    bool bAligned;
    Buffer_t Cmd;
    Buffer_t Resp;

    double dAlt, dAz;
    int nAltPos, nAzPos;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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


    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 6;
    Cmd[SRC_DEV] = PC;
    Cmd[CMD_ID] = MC_SET_POSITION;

    nErr = isAligned(bAligned);
    if(nErr)
        return nErr;

    if(m_bMountIsGEM) {     //// Equatorial //////
        // do Az (Ra)
        Cmd[DST_DEV] = AZM;
        degToSteps(dRa, nAzPos);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
        degToSteps(dDec, nAltPos);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
        m_pTsx->EqToHz(dRa, dDec, dAz, dAlt);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to dAz = %3.2f\n", timestamp, dAz);
        fprintf(Logfile, "[%s] [SkyPortalWiFi::startSlewTo] Slewing to dAlt = %3.2f\n", timestamp, dAlt);
        fflush(Logfile);
#endif
        // do Az
        Cmd[DST_DEV] = AZM;
        degToSteps(dAz, nAzPos);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
        degToSteps(dAlt, nAltPos);
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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

    return nErr;
}


int SkyPortalWiFi::startOpenSlew(const MountDriverInterface::MoveDir Dir, unsigned int nRate)
{
    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
    Buffer_t Resp;
    int nSlewRate;

    m_nOpenLoopDir = Dir;

    Cmd[0] = SOM;
    Cmd[MSG_LEN] = 3;
    Cmd[SRC_DEV] = PC;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::startOpenSlew] setting to Dir %d\n", timestamp, Dir);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::startOpenSlew] Setting rate to %d\n", timestamp, nRate);
    fflush(Logfile);
#endif

    // select rate
    nSlewRate = atoi(m_aszSlewRateNames[nRate]); // 0 to 9

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
    Buffer_t Resp;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
    return nErr;
}

int SkyPortalWiFi::isSlewToComplete(bool &bComplete)
{
    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
    Buffer_t Resp;
    unsigned char szHexMessage[LOG_BUFFER_SIZE];
    bool bAzComplete, baltComplete;

    bComplete = false;
    bAzComplete = false;
    baltComplete = false;

    if(m_Timer.GetElapsedSeconds()<2) {
        // we can't talk to the controller too often when slewing
        return nErr;
    }

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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

    if(Resp[5] == 0xFF)
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

    if(Resp[5] == 0xFF)
        bAzComplete = true;

    bComplete = bAzComplete & baltComplete;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        fprintf(Logfile, "[%s] SkyPortalWiFi::isSlewToComplete doing slow goto\n", timestamp);
        fflush(Logfile);
#endif
        nErr = startSlewTo(m_dGotoRATarget, m_dGotoDECTarget, false ); // now do a slow goto
        return nErr;
    }
    else if(bComplete & !m_bIsTracking) {
        m_dCurrentRa = m_dGotoRATarget;
        m_dCurrentDec = m_dGotoDECTarget;

        if(m_bIsParking) {
            m_bIsTracking = false;
            setTrackingRatesSteps(0, 0, 3); // stop
        }
        else {
            m_bIsTracking = true;
            if(m_bMountIsGEM)
                nErr = setTrackingRates(true, true, 0, 0); // set to sidereal
            // else not needed, it'll be set in the getRaDec for Alt/Az mount as we need to compute the tracking rate.
        }
    }

    return nErr;
}

int SkyPortalWiFi::gotoPark(double dRa, double dDec)
{
    int nErr = SKYPORTAL_OK;

    // goto Park
    nErr = startSlewTo(dRa, dDec);
    m_dParkRa = dRa;
    m_dParkDec = dDec;

    m_bIsParking = true;
    m_bIsTracking = false;

    return nErr;
}


int SkyPortalWiFi::getAtPark(bool &bParked)
{
    int nErr = SKYPORTAL_OK;
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getAtPark]\n", timestamp);
    fflush(Logfile);
#endif

    // there is no real pack position status from the mount, so do our best
    //  if this is called outside of a parking call.
    getPosition(nAzSteps, nAltSteps);
    stepsToDeg(nAltSteps, dAlt);
    stepsToDeg(nAzSteps, dAz);

    m_pTsx->HzToEq(dAz, dAlt, dRa, dDec);

    if( floor(dRa) == floor(m_dParkRa) && floor(dDec) == floor(m_dParkDec))
        bParked = true;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
    int nErr = SKYPORTAL_OK;
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
    int nErr = SKYPORTAL_OK;

    nErr = setTrackingRatesSteps(0, 0, 3);
    nErr |= moveAlt(0);
    nErr |= moveAz(0);

    return nErr;
}

int SkyPortalWiFi::moveAz(int nSteps)
{
    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
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
    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
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
    int nErr = SKYPORTAL_OK;
    Buffer_t Cmd;
    Buffer_t Resp;
    unsigned char cCmd;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nAzRate = %d\n", timestamp, nAzRate);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nAltRate = %d\n", timestamp, nAltRate);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::setTrackingRatesSteps] nDataLen = %d\n", timestamp, nDataLen);
    fflush(Logfile);
#endif

    cCmd = MC_SET_POS_GUIDERATE;
    if(nAzRate <0) {
        nAzRate = abs(nAzRate);
        cCmd = MC_SET_NEG_GUIDERATE;
    }

    // special case for sidereal south
    if(nAzRate == 0xFFFF &&  m_pTsx->latitude()<0)
        cCmd = MC_SET_NEG_GUIDERATE;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
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
    int nErr = SKYPORTAL_OK;
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
    int nErr = SKYPORTAL_OK;
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

void SkyPortalWiFi::stepsToDeg(int nSteps, double &dDeg)
{

    dDeg = double(nSteps) / STEPS_PER_DEGREE;
}

void SkyPortalWiFi::degToSteps(double dDeg, int &nSteps)
{
    nSteps = int(dDeg * STEPS_PER_DEGREE);
}


void SkyPortalWiFi::fixAltSteps(int &nAltSteps)
{
    // return alt encoder adjusted to -90...+90
    if (nAltSteps > STEPS_PER_REVOLUTION / 2)
        nAltSteps -= STEPS_PER_REVOLUTION;
}

void SkyPortalWiFi::fixAzSteps(int &nAzSteps)
{
    // wrap around
    nAzSteps %= STEPS_PER_REVOLUTION;
}



int SkyPortalWiFi::getFixedAlt(int nAltSteps)
{
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFixedAlt] nAltSteps = %d\n", timestamp, nAltSteps);
    fflush(Logfile);
#endif

    // return alt encoder adjusted to -90...+90
    if (nAltSteps > STEPS_PER_REVOLUTION / 2)
        nAltSteps -= STEPS_PER_REVOLUTION;

#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFixedAlt] Fixed nAltSteps = %d\n", timestamp, nAltSteps);
    fflush(Logfile);
#endif

    return nAltSteps;

}

int SkyPortalWiFi::getFixedAz(int nAzSteps)
{
#if defined SKYPORTAL_DEBUG && SKYPORTAL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFixedAz] nAzSteps = %d\n", timestamp, nAzSteps);
    fprintf(Logfile, "[%s] [SkyPortalWiFi::getFixedAz] nAzSteps %% STEPS_PER_REVOLUTION = %d\n", timestamp, nAzSteps % STEPS_PER_REVOLUTION);
    fflush(Logfile);
#endif

    // wrap around
    return nAzSteps %= STEPS_PER_REVOLUTION;
}


int SkyPortalWiFi::parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = SKYPORTAL_OK;
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


