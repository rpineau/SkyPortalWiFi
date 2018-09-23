#include "x2mount.h"

X2Mount::X2Mount(const char* pszDriverSelection,
				 const int& nInstanceIndex,
				 SerXInterface					* pSerX,
				 TheSkyXFacadeForDriversInterface	* pTheSkyX,
				 SleeperInterface					* pSleeper,
				 BasicIniUtilInterface			* pIniUtil,
				 MutexInterface					* pIOMutex,
				 TickCountInterface				* pTickCount)
{
    double dAz, dAlt;
    double dRa, dDec;

    m_nPrivateMulitInstanceIndex	= nInstanceIndex;
	m_pSerX							= pSerX;
	m_pTheSkyXForMounts				= pTheSkyX;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;
	
#ifdef SkyPortalWiFi_X2_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\SkyPortalWiFi_X2_Logfile.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SkyPortalWiFi_X2_Logfile.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SkyPortalWiFi_X2_Logfile.txt";
#endif
	LogFile = fopen(m_sLogfilePath.c_str(), "w");
#endif
	
	
	m_bSynced = false;
	m_bParked = false;
    m_bLinked = false;

    mSkyPortalWiFi.setSerxPointer(m_pSerX);
    mSkyPortalWiFi.setTSX(m_pTheSkyXForMounts);
    mSkyPortalWiFi.setSleeper(m_pSleeper);

    m_CurrentRateIndex = 0;
    m_nParity = SerXInterface::B_NOPARITY;
    
	// Read the current stored values for the settings
    // set to equatorial by default as this is the easiest to deal with
	if (m_pIniUtil)
	{
        mSkyPortalWiFi.setMountMode( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_MOUNT_IS_GEM, true));

        dAz = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, 0);
        dAlt = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_PARK_ALT, 0);
        m_pTheSkyXForMounts->HzToEq(dAz, dAlt, dRa, dDec);
        mSkyPortalWiFi.setParkPosition(dRa, dDec);

        // mSkyPortalWiFi.setCordWrapEnabled(m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_CORD_WRAP, false));
        // mSkyPortalWiFi.setCordWrapPos(m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_CORD_WRAP_POS, 0.0));
	}

}

X2Mount::~X2Mount()
{
	// Write the stored values

    if(m_bLinked)
        mSkyPortalWiFi.Disconnect();
    
    if (m_pSerX)
		delete m_pSerX;
	if (m_pTheSkyXForMounts)
		delete m_pTheSkyXForMounts;
	if (m_pSleeper)
		delete m_pSleeper;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pIOMutex)
		delete m_pIOMutex;
	if (m_pTickCount)
		delete m_pTickCount;
	
#ifdef SkyPortalWiFi_X2_DEBUG
	// Close LogFile
	if (LogFile) {
        fflush(LogFile);
		fclose(LogFile);
	}
#endif
	
}

int X2Mount::queryAbstraction(const char* pszName, void** ppVal)
{
	*ppVal = NULL;
	
	if (!strcmp(pszName, SyncMountInterface_Name))
	    *ppVal = dynamic_cast<SyncMountInterface*>(this);
	if (!strcmp(pszName, SlewToInterface_Name))
		*ppVal = dynamic_cast<SlewToInterface*>(this);
	else if (!strcmp(pszName, AsymmetricalEquatorialInterface_Name))
		*ppVal = dynamic_cast<AsymmetricalEquatorialInterface*>(this);
	else if (!strcmp(pszName, OpenLoopMoveInterface_Name))
		*ppVal = dynamic_cast<OpenLoopMoveInterface*>(this);
	else if (!strcmp(pszName, NeedsRefractionInterface_Name))
		*ppVal = dynamic_cast<NeedsRefractionInterface*>(this);
	else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
		*ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
	else if (!strcmp(pszName, X2GUIEventInterface_Name))
		*ppVal = dynamic_cast<X2GUIEventInterface*>(this);
	else if (!strcmp(pszName, TrackingRatesInterface_Name))
		*ppVal = dynamic_cast<TrackingRatesInterface*>(this);
	else if (!strcmp(pszName, ParkInterface_Name))
		*ppVal = dynamic_cast<ParkInterface*>(this);
	else if (!strcmp(pszName, UnparkInterface_Name))
		*ppVal = dynamic_cast<UnparkInterface*>(this);
    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);
    else if (!strcmp(pszName, DriverSlewsToParkPositionInterface_Name))
        *ppVal = dynamic_cast<DriverSlewsToParkPositionInterface*>(this);
	return SB_OK;
}

#pragma mark - OpenLoopMoveInterface

int X2Mount::startOpenLoopMove(const MountDriverInterface::MoveDir& Dir, const int& nRateIndex)
{
    int nErr = SB_OK;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

	m_CurrentRateIndex = nRateIndex;
#ifdef SkyPortalWiFi_X2_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		char *timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] startOpenLoopMove called Dir: %d , Rate: %d\n", timestamp, Dir, nRateIndex);
        fflush(LogFile);
	}
#endif

    nErr = mSkyPortalWiFi.startOpenSlew(Dir, nRateIndex);
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] startOpenLoopMove ERROR %d\n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        return ERR_CMDFAILED;
    }
    return SB_OK;
}

int X2Mount::endOpenLoopMove(void)
{
	int nErr = SB_OK;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

#ifdef SkyPortalWiFi_X2_DEBUG
	if (LogFile){
		time_t ltime = time(NULL);
		char *timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] endOpenLoopMove Called\n", timestamp);
        fflush(LogFile);
	}
#endif

    nErr = mSkyPortalWiFi.stopOpenLoopMove();
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] endOpenLoopMove ERROR %d\n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        return ERR_CMDFAILED;
    }
    return nErr;
}

int X2Mount::rateCountOpenLoopMove(void) const
{
    X2Mount* pMe = (X2Mount*)this;

    X2MutexLocker ml(pMe->GetMutex());
	return pMe->mSkyPortalWiFi.getNbSlewRates();
}

int X2Mount::rateNameFromIndexOpenLoopMove(const int& nZeroBasedIndex, char* pszOut, const int& nOutMaxSize)
{
    int nErr = SB_OK;
    nErr = mSkyPortalWiFi.getRateName(nZeroBasedIndex, pszOut, nOutMaxSize);
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] rateNameFromIndexOpenLoopMove ERROR %d\n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        return ERR_CMDFAILED;
    }
    return nErr;
}

int X2Mount::rateIndexOpenLoopMove(void)
{
	return m_CurrentRateIndex;
}

#pragma mark - UI binding

int X2Mount::execModalSettingsDialog(void)
{
	int nErr = SB_OK;
	X2ModalUIUtil uiutil(this, m_pTheSkyXForMounts);
	X2GUIInterface*					ui = uiutil.X2UI();
	X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
	bool bPressedOK = false;
    char szTmpBuf[SERIAL_BUFFER_SIZE];
    bool bIsGem = false;
    bool bCordWrap = false;
    double dCordWrapPos;

    if (NULL == ui) return ERR_POINTER;
	
	if ((nErr = ui->loadUserInterface("SkyPortalWiFi.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
		return nErr;
	
	if (NULL == (dx = uiutil.X2DX())) {
		return ERR_POINTER;
	}

    memset(szTmpBuf,0,SERIAL_BUFFER_SIZE);

    X2MutexLocker ml(GetMutex());

    if(mSkyPortalWiFi.isMountGem())
        dx->setChecked("radioButton_2", true);
    else
        dx->setChecked("radioButton", true);

    // Set values in the userinterface
    if(m_bLinked) {
        dx->setEnabled("enableCordWrap",true);
        dx->setEnabled("cordWrapPosition",true);
    }
    else {
        dx->setEnabled("enableCordWrap",false);
        dx->setEnabled("cordWrapPosition",false);
    }

	//Display the user interface
	if ((nErr = ui->exec(bPressedOK)))
		return nErr;
	
	//Retreive values from the user interface
	if (bPressedOK) {
        bIsGem = dx->isChecked("radioButton_2");
        bCordWrap = dx->isChecked("enableCordWrap");
        m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_MOUNT_IS_GEM, bIsGem);
        m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_CORD_WRAP, bCordWrap);
        if(bCordWrap) {
            dx->propertyDouble("cordWrapPosition", "value", dCordWrapPos);
            m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_CORD_WRAP_POS, dCordWrapPos);
        }
        mSkyPortalWiFi.setMountMode(bIsGem);
        // mSkyPortalWiFi.setCordWrapEnabled(bCordWrap);
        // mSkyPortalWiFi.setCordWrapPos(dCordWrapPos);
	}
	return nErr;
}

void X2Mount::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    int nErr = SB_OK;
    char szTmpBuf[SERIAL_BUFFER_SIZE];

    if(!m_bLinked)
        return ;

    memset(szTmpBuf,0,SERIAL_BUFFER_SIZE);

#ifdef SkyPortalWiFi_X2_DEBUG
	time_t ltime;
	char *timestamp;
	if (LogFile) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] uievent %s\n", timestamp, pszEvent);
        fflush(LogFile);
	}
#endif
	if (!strcmp(pszEvent, "on_timer")) {
    }


	return;
}

#pragma mark - LinkInterface
int X2Mount::establishLink(void)
{
    int nErr;
    char szPort[DRIVER_MAX_STRING];

	X2MutexLocker ml(GetMutex());
	// get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);

	nErr =  mSkyPortalWiFi.Connect(szPort);
    if(nErr) {
        m_bLinked = false;
    }
    else {
        m_bLinked = true;
    }
    return nErr;
}

int X2Mount::terminateLink(void)
{
    int nErr = SB_OK;

	X2MutexLocker ml(GetMutex());

    nErr = mSkyPortalWiFi.Disconnect();
    m_bLinked = false;

    return nErr;
}

bool X2Mount::isLinked(void) const
{
	return mSkyPortalWiFi.isConnected();
}

bool X2Mount::isEstablishLinkAbortable(void) const
{
    return false;
}

#pragma mark - AbstractDriverInfo

void	X2Mount::driverInfoDetailedInfo(BasicStringInterface& str) const
{
	str = "SkyPortal WiFi X2 plugin by Rodolphe Pineau";
}

double	X2Mount::driverInfoVersion(void) const
{
	return DRIVER_VERSION;
}

void X2Mount::deviceInfoNameShort(BasicStringInterface& str) const
{
    str = "SkyPortal WiFi";
}
void X2Mount::deviceInfoNameLong(BasicStringInterface& str) const
{
	str = "SkyPortal WiFi";
	
}
void X2Mount::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
	str = "Celestron SkyPortal Wifi";
}

void X2Mount::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
    if(m_bLinked) {
        int nErr  = SB_OK;
        char cFirmware[SERIAL_BUFFER_SIZE];
        X2MutexLocker ml(GetMutex());
        nErr = mSkyPortalWiFi.getFirmwareVersion(cFirmware, SERIAL_BUFFER_SIZE);
        if(nErr) {
            str = "Error reading firmware";
            return;
        }
        str = cFirmware;
    }
    else
        str = "Not connected";
}
void X2Mount::deviceInfoModel(BasicStringInterface& str)
{
    str = "SKyPortal WiFi";
}

#pragma mark - Common Mount specifics
int X2Mount::raDec(double& ra, double& dec, const bool& bCached)
{
	int nErr = 0;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

	// Get the RA and DEC from the mount
	nErr = mSkyPortalWiFi.getRaAndDec(ra, dec);
    if(nErr)
        nErr = ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] raDec Called. Ra : %f , Dec : %f \n", timestamp, ra, dec);
        fprintf(LogFile, "[%s] nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

	return nErr;
}

int X2Mount::abort()
{
    int nErr = SB_OK;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

#ifdef SkyPortalWiFi_X2_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		char *timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] abort Called\n", timestamp);
        fflush(LogFile);
	}
#endif

    nErr = mSkyPortalWiFi.Abort();
    if(nErr)
        nErr = ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] Abort nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

    return nErr;
}

int X2Mount::startSlewTo(const double& dRa, const double& dDec)
{
	int nErr = SB_OK;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

#ifdef SkyPortalWiFi_X2_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		char *timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] startSlewTo Called %f %f\n", timestamp, dRa, dDec);
        fflush(LogFile);
	}
#endif
    nErr = mSkyPortalWiFi.startSlewTo(dRa, dDec);
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] startSlewTo nErr = %d \n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int X2Mount::isCompleteSlewTo(bool& bComplete) const
{
    int nErr = SB_OK;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2Mount* pMe = (X2Mount*)this;
    X2MutexLocker ml(pMe->GetMutex());

    nErr = pMe->mSkyPortalWiFi.isSlewToComplete(bComplete);
    if(nErr)
        return ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] isCompleteSlewTo nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

	return nErr;
}

int X2Mount::endSlewTo(void)
{
#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] endSlewTo Called\n", timestamp);
        fflush(LogFile);
    }
#endif
    return SB_OK;
}


int X2Mount::syncMount(const double& ra, const double& dec)
{
	int nErr = SB_OK;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] syncMount Called : %f\t%f\n", timestamp, ra, dec);
        fflush(LogFile);
    }
#endif

    nErr = mSkyPortalWiFi.syncTo(ra, dec);
    if(nErr)
        nErr = ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] syncMount nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

    return nErr;
}

bool X2Mount::isSynced(void)
{
    int nErr;

    if(!m_bLinked)
        return false;

    X2MutexLocker ml(GetMutex());

   nErr = mSkyPortalWiFi.isAligned(m_bSynced);

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] isSynced Called : m_bSynced = %s\n", timestamp, m_bSynced?"true":"false");
        fprintf(LogFile, "[%s] isSynced nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

    return m_bSynced;
}

#pragma mark - TrackingRatesInterface
int X2Mount::setTrackingRates(const bool& bTrackingOn, const bool& bIgnoreRates, const double& dRaRateArcSecPerSec, const double& dDecRateArcSecPerSec)
{
    int nErr = SB_OK;
    double dTrackRaArcSecPerMin;
    double dTrackDecArcSecPerMin;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    dTrackRaArcSecPerMin = dRaRateArcSecPerSec * 60;
    dTrackDecArcSecPerMin = dDecRateArcSecPerSec * 60;

    nErr = mSkyPortalWiFi.setTrackingRates(bTrackingOn, bIgnoreRates, dTrackRaArcSecPerMin, dTrackDecArcSecPerMin);
#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] setTrackingRates Called. Tracking On: %s , Ra rate : %f , Dec rate: %f\n", timestamp, bTrackingOn?"true":"false", dRaRateArcSecPerSec, dDecRateArcSecPerSec);
        fflush(LogFile);
    }
#endif
    if(nErr)
        return ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] setTrackingRates nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

    return nErr;
}

int X2Mount::trackingRates(bool& bTrackingOn, double& dRaRateArcSecPerSec, double& dDecRateArcSecPerSec)
{
    int nErr = SB_OK;
    double dTrackRaArcSecPerMin;
    double dTrackDecArcSecPerMin;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    nErr = mSkyPortalWiFi.getTrackRates(bTrackingOn, dTrackRaArcSecPerMin, dTrackDecArcSecPerMin);
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] trackingRates  mSkyPortalWiFi.getTrackRates nErr = %d \n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        return ERR_CMDFAILED;
    }
    dRaRateArcSecPerSec = dTrackRaArcSecPerMin / 60;
    dDecRateArcSecPerSec = dTrackDecArcSecPerMin / 60;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] trackingRates Called. Tracking On: %d , Ra rate : %f , Dec rate: %f\n", timestamp, bTrackingOn, dRaRateArcSecPerSec, dDecRateArcSecPerSec);
        fflush(LogFile);
    }
#endif

	return nErr;
}

int X2Mount::siderealTrackingOn()
{
    int nErr = SB_OK;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] siderealTrackingOn Called \n", timestamp);
        fflush(LogFile);
    }
#endif

    nErr = setTrackingRates( true, true, 0.0, 0.0);
    if(nErr)
        return ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] siderealTrackingOn nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

    return nErr;
}

int X2Mount::trackingOff()
{
    int nErr = SB_OK;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] trackingOff Called \n", timestamp);
        fflush(LogFile);
    }
#endif
    nErr = setTrackingRates( false, true, 0.0, 0.0);
    if(nErr) {
        nErr = ERR_CMDFAILED;
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] trackingOff nErr = %d \n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
    }
    return nErr;
}

#pragma mark - NeedsRefractionInterface
bool X2Mount::needsRefactionAdjustments(void)
{
    return true;
}

#pragma mark - Parking Interface
bool X2Mount::isParked(void)
{
    int nErr;
    bool bTrackingOn;
    bool bIsPArked;
    double dTrackRaArcSecPerMin, dTrackDecArcSecPerMin;

    if(!m_bLinked)
        return false;

    X2MutexLocker ml(GetMutex());

    nErr = mSkyPortalWiFi.getAtPark(bIsPArked);
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] isParked mSkyPortalWiFi.getAtPark nErr = %d \n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        return false;
    }
    if(!bIsPArked) // not parked
        return false;

    // get tracking state.
    nErr = mSkyPortalWiFi.getTrackRates(bTrackingOn, dTrackRaArcSecPerMin, dTrackDecArcSecPerMin);
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] isParked mSkyPortalWiFi.getTrackRates nErr = %d \n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        return false;
    }
    // if AtPark and tracking is off, then we're parked, if not then we're unparked.
    if(bIsPArked && !bTrackingOn)
        m_bParked = true;
    else
        m_bParked = false;
    return m_bParked;
}

int X2Mount::startPark(const double& dAz, const double& dAlt)
{
	double dRa, dDec;
	int nErr = SB_OK;

    if(!m_bLinked)
        return ERR_NOLINK;
	
	X2MutexLocker ml(GetMutex());

    // save park position as the mount doesn't have proper park functions
    m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, dAz);
    m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_PARK_ALT, dAlt);

    m_pTheSkyXForMounts->HzToEq(dAz, dAlt, dRa, dDec);
    
    // goto park
    nErr = mSkyPortalWiFi.gotoPark(dRa, dDec);
    if(nErr)
        nErr = ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] startPark  mSkyPortalWiFi.gotoPark nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

	return nErr;
}


int X2Mount::isCompletePark(bool& bComplete) const
{
    int nErr = SB_OK;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2Mount* pMe = (X2Mount*)this;

    X2MutexLocker ml(pMe ->GetMutex());

#ifdef SkyPortalWiFi_X2_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		char *timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(LogFile, "[%s] isCompletePark Called\n", timestamp);
        fflush(LogFile);
	}
#endif
    nErr = pMe->mSkyPortalWiFi.getAtPark(bComplete);
    if(nErr)
        nErr = ERR_CMDFAILED;

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] isCompletePark  mSkyPortalWiFi.getAtPark nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

	return nErr;
}

int X2Mount::endPark(void)
{
    return SB_OK;
}

int X2Mount::startUnpark(void)
{
    int nErr = SB_OK;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    nErr = mSkyPortalWiFi.unPark();
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] startUnpark : mSkyPortalWiFi.unPark() failled !\n", timestamp);
            fflush(LogFile);
        }
#endif
        nErr = ERR_CMDFAILED;
    }
    m_bParked = false;
    return nErr;
}

/*!Called to monitor the unpark process.
 \param bComplete Set to true if the unpark is complete, otherwise set to false.
*/
int X2Mount::isCompleteUnpark(bool& bComplete) const
{
    int nErr;
    bool bIsParked;
    bool bTrackingOn;
    double dTrackRaArcSecPerMin, dTrackDecArcSecPerMin;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2Mount* pMe = (X2Mount*)this;

    X2MutexLocker ml(pMe ->GetMutex());

    bComplete = false;

    nErr = pMe->mSkyPortalWiFi.getAtPark(bIsParked);
    if(nErr) {
#ifdef SkyPortalWiFi_X2_DEBUG
        if (LogFile) {
            time_t ltime = time(NULL);
            char *timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(LogFile, "[%s] isCompleteUnpark  mSkyPortalWiFi.getAtPark nErr = %d \n", timestamp, nErr);
            fflush(LogFile);
        }
#endif
        nErr = ERR_CMDFAILED;
    }
    if(!bIsParked) { // no longer parked.
        bComplete = true;
        pMe->m_bParked = false;
        return nErr;
    }

    // if we're still at the park position
    // get tracking state. If tracking is off, then we're parked, if not then we're unparked.
    nErr = pMe->mSkyPortalWiFi.getTrackRates(bTrackingOn, dTrackRaArcSecPerMin, dTrackDecArcSecPerMin);
    if(nErr)
        nErr = ERR_CMDFAILED;
#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] isCompleteUnpark  mSkyPortalWiFi.getTrackRates nErr = %d \n", timestamp, nErr);
        fflush(LogFile);
    }
#endif

    if(bTrackingOn) {
        bComplete = true;
        pMe->m_bParked = false;
    }
    else {
        bComplete = false;
        pMe->m_bParked = true;
    }
	return SB_OK;
}

/*!Called once the unpark is complete.
 This is called once for every corresponding startUnpark() allowing software implementations of unpark.
 */
int		X2Mount::endUnpark(void)
{
	return SB_OK;
}

#pragma mark - AsymmetricalEquatorialInterface

bool X2Mount::knowsBeyondThePole()
{
    return true;
}

int X2Mount::beyondThePole(bool& bYes) {
	// bYes = mSkyPortalWiFi.GetIsBeyondThePole();
	return SB_OK;
}


double X2Mount::flipHourAngle() {
#ifdef SkyPortalWiFi_X2_DEBUG
	if (LogFile) {
		time_t ltime = time(NULL);
		char *timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		// fprintf(LogFile, "[%s] flipHourAngle called\n", timestamp);
        fflush(LogFile);
	}
#endif

	return 0.0;
}


int X2Mount::gemLimits(double& dHoursEast, double& dHoursWest)
{
    int nErr = SB_OK;
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    nErr = mSkyPortalWiFi.getLimits(dHoursEast, dHoursWest);

#ifdef SkyPortalWiFi_X2_DEBUG
    if (LogFile) {
        time_t ltime = time(NULL);
        char *timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(LogFile, "[%s] gemLimits mSkyPortalWiFi.getLimits nErr = %d\n", timestamp, nErr);
        fprintf(LogFile, "[%s] gemLimits dHoursEast = %f\n", timestamp, dHoursEast);
        fprintf(LogFile, "[%s] gemLimits dHoursWest = %f\n", timestamp, dHoursWest);
        fflush(LogFile);
    }
#endif
    // temp debugging.
	dHoursEast = 0.0;
	dHoursWest = 0.0;
	return SB_OK;
}

MountTypeInterface::Type X2Mount::mountType()
{
    return mSkyPortalWiFi.mountType();
}

#pragma mark - SerialPortParams2Interface

void X2Mount::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Mount::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORT_NAME, pszPort);

}


void X2Mount::portNameOnToCharPtr(char* pszPort, const unsigned int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize,DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORT_NAME, pszPort, pszPort, nMaxSize);

}




